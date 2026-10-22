// CI test harness: runs a program under a seccomp filter that, like Android's app sandbox, kills with SIGSYS
// (SECCOMP_RET_TRAP) on the ARM64 system calls Android rejects and glibc/Wine are known to make:
// set_robust_list (99), accept (202), faccessat2 (439). The filter is inherited by every child process,
// so `seccomp_sim sch1levo-runtime prefix-init` behaves like the app on the tablet for these calls.
// Usage: seccomp_sim <program> [args...]
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#define TRAP_IF(nr) BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (nr), 0, 1), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP)

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: seccomp_sim <program> [args...]\n");
        return 64;
    }
    struct sock_filter filter[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_AARCH64, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
        TRAP_IF(99),   // set_robust_list
        TRAP_IF(202),  // accept
        TRAP_IF(439),  // faccessat2
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };
    struct sock_fprog prog = {(unsigned short)(sizeof filter / sizeof filter[0]), filter};
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0 || prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        perror("seccomp_sim: installing filter");
        return 1;
    }
    execv(argv[1], argv + 1);
    perror("seccomp_sim: execv");
    return 127;
}
