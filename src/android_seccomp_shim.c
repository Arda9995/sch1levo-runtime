// Linked into box64. Android's app sandbox (seccomp) rejects some system calls that glibc on ARM64 makes,
// by sending SIGSYS, which kills the process ("Bad system call"). Seen on Android 16 (app sandbox):
//   99  set_robust_list  glibc thread setup      -> answer ENOSYS (glibc's own calls are patched out: patch-glibc.py)
//   202 accept           wineserver's main loop  -> run accept4(fd, addr, len, 0) instead (Android allows accept4)
//   439 faccessat2       file permission checks  -> answer ENOSYS (glibc falls back to faccessat)
// Any other blocked call is answered with ENOSYS and named once on stderr, so it shows in the app's diagnostics
// instead of killing Wine. If the emulated program installs its own SIGSYS handler, it is kept and called for
// calls this shim does not handle, so this handler stays first.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

#if defined(__aarch64__)

typedef int (*sigaction_fn)(int, const struct sigaction *, struct sigaction *);

static sigaction_fn real_sigaction;
static struct sigaction chained;   // SIGSYS action requested by the emulated program, if any
static int have_chained;
static unsigned char reported[1024];

static long raw_syscall4(long nr, long a, long b, long c, long d) {
    register long x8 __asm__("x8") = nr;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}

static void report_once(int nr) {
    if (nr < 0 || nr >= (int)sizeof reported || reported[nr]) return;
    reported[nr] = 1;
    char msg[64] = "sch1levo: sandbox blocked syscall ";
    size_t len = strlen(msg);
    char digits[12];
    int n = 0;
    do { digits[n++] = (char)('0' + nr % 10); nr /= 10; } while (nr > 0 && n < (int)sizeof digits);
    while (n > 0) msg[len++] = digits[--n];
    msg[len++] = '\n';
    (void)!write(2, msg, len);
}

static void on_sigsys(int sig, siginfo_t *si, void *ctx) {
    ucontext_t *uc = ctx;
    unsigned long long *r = uc->uc_mcontext.regs;
    int saved_errno = errno;
    switch (si->si_syscall) {
        case __NR_accept:
            r[0] = (unsigned long long)raw_syscall4(__NR_accept4, (long)r[0], (long)r[1], (long)r[2], 0);
            break;
        case __NR_set_robust_list:
        case 439: /* faccessat2 */
            r[0] = (unsigned long long)-ENOSYS;
            break;
        default:
            if (have_chained && (chained.sa_flags & SA_SIGINFO) && chained.sa_sigaction) {
                chained.sa_sigaction(sig, si, ctx);
            } else if (have_chained && chained.sa_handler != SIG_DFL && chained.sa_handler != SIG_IGN) {
                chained.sa_handler(sig);
            } else {
                report_once(si->si_syscall);
                r[0] = (unsigned long long)-ENOSYS;
            }
            break;
    }
    errno = saved_errno;
}

// Keeps this handler installed for SIGSYS; requests from box64 or the emulated program are remembered and chained.
int sigaction(int sig, const struct sigaction *act, struct sigaction *old) {
    if (!real_sigaction) real_sigaction = (sigaction_fn)dlsym(RTLD_NEXT, "sigaction");
    if (sig != SIGSYS) return real_sigaction(sig, act, old);
    if (old) {
        if (have_chained) *old = chained;
        else { memset(old, 0, sizeof *old); old->sa_handler = SIG_DFL; }
    }
    if (act) {
        chained = *act;
        have_chained = act->sa_handler != SIG_DFL;
    }
    return 0;
}

__attribute__((constructor(101)))
static void install_sigsys_handler(void) {
    real_sigaction = (sigaction_fn)dlsym(RTLD_NEXT, "sigaction");
    if (!real_sigaction) return;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_sigsys;
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    real_sigaction(SIGSYS, &sa, NULL);
}

#endif
