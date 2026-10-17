/*
 * Sch1Mac steamwebhelper wrapper (MIT).
 *
 * Installed as steamwebhelper.exe in every <Steam>\bin\cef\cef.win* directory, after the original has been renamed to
 * steamwebhelper_real.exe. Re-executes the original with its exact command-line tail plus
 * "--disable-gpu --single-process", which avoids the black Steam window under Wine 11 on macOS
 * and a winsock TLS failure in CEF's network service. Waits for the child and returns its exit code.
 * No logging, no environment reads, no file writes.
 */
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>

static const wchar_t REAL_NAME[] = L"steamwebhelper_real.exe";
static const wchar_t EXTRA_FLAGS[] = L" --disable-gpu --single-process";

/* Returns a pointer to the command line after the program-name token (quoted or bare) and blanks. */
static const wchar_t *skip_program_name(const wchar_t *cmd)
{
    if (*cmd == L'"') {
        cmd++;
        while (*cmd && *cmd != L'"') cmd++;
        if (*cmd == L'"') cmd++;
    } else {
        while (*cmd && *cmd != L' ' && *cmd != L'\t') cmd++;
    }
    while (*cmd == L' ' || *cmd == L'\t') cmd++;
    return cmd;
}

int wmain(void)
{
    wchar_t dir[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(NULL, dir, (DWORD)(sizeof(dir) / sizeof(dir[0])));
    if (n == 0 || n >= sizeof(dir) / sizeof(dir[0])) return 126;
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (!slash) return 126;
    slash[1] = L'\0';

    size_t dir_len = wcslen(dir);
    wchar_t *real_path = malloc((dir_len + wcslen(REAL_NAME) + 1) * sizeof(wchar_t));
    if (!real_path) return 125;
    wcscpy(real_path, dir);
    wcscat(real_path, REAL_NAME);

    const wchar_t *rest = skip_program_name(GetCommandLineW());
    size_t len = wcslen(real_path) + wcslen(rest) + wcslen(EXTRA_FLAGS) + 4;
    wchar_t *line = malloc(len * sizeof(wchar_t));
    if (!line) return 125;
    wcscpy(line, L"\"");
    wcscat(line, real_path);
    wcscat(line, L"\" ");
    wcscat(line, rest);
    wcscat(line, EXTRA_FLAGS);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(real_path, line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) return 127;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    free(line);
    free(real_path);
    return (int)code;
}
