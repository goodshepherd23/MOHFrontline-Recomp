/* MOHFrontline.exe -- launcher for the Medal of Honor: Frontline (Recompiled) kit.
 *
 * Player data (the game files, the built module, settings and saves) lives in
 * %LOCALAPPDATA%\MOHFrontline-Recomp, outside the app folder, so a launcher update that replaces
 * the app folder cannot wipe it. A file named portable.txt beside MOHFrontline.exe keeps everything
 * in the app folder instead.
 *
 * 1. Runs kit\setup.ps1 when the game has not been built for this kit version yet (it asks for the
 *    disc the first time).
 * 2. Starts runtime\moderngekko-run.exe with the built module, maximized and focused, on the fastest
 *    CPU cores. Other arguments are passed to the runtime unchanged.
 *
 * Usage: MOHFrontline.exe            play (sets up first if needed)
 *        MOHFrontline.exe --setup    choose another disc
 *        MOHFrontline.exe --rebuild  rebuild the game from the disc
 * GPL-3.0, see LICENSE. */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define KIT_STR_(x) #x
#define KIT_STR(x) KIT_STR_(x)
#define KIT_WIDE_(s) L##s
#define KIT_WIDE(s) KIT_WIDE_(s)
#ifdef KIT_VER
#define KIT_VERSION KIT_WIDE(KIT_STR(KIT_VER))
#else
#define KIT_VERSION L"dev"
#endif
#define PATHMAX 32768
#define TITLE L"Medal of Honor: Frontline"

static wchar_t root[PATHMAX];   /* app folder: MOHFrontline.exe, kit\, runtime\ */
static wchar_t data[PATHMAX];   /* data folder: game\, module\, user\, tools\, build\ */

static void path(wchar_t *out, const wchar_t *rel) { swprintf(out, PATHMAX, L"%ls\\%ls", data, rel); }
static int exists(const wchar_t *rel) { wchar_t p[PATHMAX]; path(p, rel); return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }

static void note(const wchar_t *msg) {   /* appends to launcher.log in the data folder */
    wchar_t p[PATHMAX]; path(p, L"launcher.log");
    FILE *f = _wfopen(p, L"a, ccs=UTF-8"); if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fwprintf(f, L"%04u-%02u-%02u %02u:%02u:%02u %ls\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, msg);
    fclose(f);
}
static void fail(const wchar_t *msg) { note(msg); MessageBoxW(NULL, msg, TITLE, MB_ICONERROR); }

static void find_data(void) {
    wchar_t p[PATHMAX], local[PATHMAX];
    swprintf(p, PATHMAX, L"%ls\\portable.txt", root);
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, PATHMAX);
    if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES || !n || n >= PATHMAX) { wcscpy(data, root); return; }
    swprintf(data, PATHMAX, L"%ls\\MOHFrontline-Recomp", local);
    if (!CreateDirectoryW(data, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) wcscpy(data, root);
}

/* module\.kit-build holds the kit version; setup writes it last. */
static int built_for_this_kit(void) {
    wchar_t p[PATHMAX], v[64] = L"";
    path(p, L"module\\.kit-build");
    FILE *f = _wfopen(p, L"r");
    if (!f) return 0;
    int ok = fwscanf(f, L"%63ls", v) == 1 && !wcscmp(v, KIT_VERSION);
    fclose(f);
    return ok && exists(L"module\\gGMFE69_recomp.dll") && exists(L"game\\sys\\main.dol") &&
           exists(L"game\\files\\Moh2RelGC.elf");
}

static int run_setup(const wchar_t *extra) {
    static wchar_t cmd[3 * PATHMAX];
    STARTUPINFOW si = {0}; PROCESS_INFORMATION pi = {0}; si.cb = sizeof si;
    swprintf(cmd, sizeof cmd / sizeof *cmd,
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%ls\\kit\\setup.ps1\" -Root \"%ls\" -Data \"%ls\" %ls",
        root, root, data, extra);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, root, &si, &pi)) {
        fail(L"Could not start the setup (PowerShell is required)."); return 0;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess);
    if (code) {
        swprintf(cmd, PATHMAX, L"Setup did not finish. The details are in\n%ls\\setup.log\n\nRun Medal of Honor: Frontline again to retry.", data);
        fail(cmd);
    }
    return code == 0;
}

/* The guest CPU runs on one host thread, so keep the game on the fastest cores: those in the
 * highest efficiency class (all cores on a CPU without hybrid cores). Also opts the game out of
 * Windows' power throttling. (From the Baroque and deSPIRIA kits' launchers.) */
static void place_on_fast_cores(HANDLE process) {
    ULONG len = 0; DWORD_PTR mask = 0; BYTE best = 0;
    GetSystemCpuSetInformation(NULL, 0, &len, process, 0);
    BYTE *buf = len ? HeapAlloc(GetProcessHeap(), 0, len) : NULL;
    if (buf && GetSystemCpuSetInformation((PSYSTEM_CPU_SET_INFORMATION)buf, len, &len, process, 0)) {
        for (int pass = 0; pass < 2; pass++)
            for (ULONG off = 0; off < len; ) {
                PSYSTEM_CPU_SET_INFORMATION c = (PSYSTEM_CPU_SET_INFORMATION)(buf + off);
                if (c->Type == CpuSetInformation && c->CpuSet.Group == 0 &&
                    c->CpuSet.LogicalProcessorIndex < sizeof(DWORD_PTR) * 8) {
                    if (pass == 0) { if (c->CpuSet.EfficiencyClass > best) best = c->CpuSet.EfficiencyClass; }
                    else if (c->CpuSet.EfficiencyClass == best) mask |= (DWORD_PTR)1 << c->CpuSet.LogicalProcessorIndex;
                }
                off += c->Size;
            }
    }
    if (buf) HeapFree(GetProcessHeap(), 0, buf);
    if (mask) SetProcessAffinityMask(process, mask);
    SetPriorityClass(process, ABOVE_NORMAL_PRIORITY_CLASS);
    PROCESS_POWER_THROTTLING_STATE t = { PROCESS_POWER_THROTTLING_CURRENT_VERSION,
                                          PROCESS_POWER_THROTTLING_EXECUTION_SPEED, 0 };
    SetProcessInformation(process, ProcessPowerThrottling, &t, sizeof t);
}

/* The runtime always faults in a window-teardown callback after it has shut down cleanly
 * (0xC000041D, STATUS_FATAL_USER_CALLBACK_EXCEPTION). Its log then ends with the static
 * recompiler's "shutdown:" line; only a fault without that line is a real crash. */
static int clean_shutdown(const wchar_t *log) {
    FILE *f = _wfopen(log, L"rb");
    if (!f) return 0;
    static char buf[1 << 16];
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, size > (long)sizeof buf - 1 ? size - (long)(sizeof buf - 1) : 0, SEEK_SET);
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    return strstr(buf, "[staticrecomp] shutdown:") != NULL;
}

static DWORD g_pid;
static HWND g_window;
static BOOL CALLBACK find_window(HWND w, LPARAM lp) {
    (void)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == g_pid && IsWindowVisible(w) && GetWindow(w, GW_OWNER) == NULL) { g_window = w; return FALSE; }
    return TRUE;
}

/* Drops this launcher's own switches; everything else goes to the runtime. */
static void runtime_args(const wchar_t *args, wchar_t *out, size_t cap) {
    out[0] = 0;
    wchar_t buf[PATHMAX]; wcsncpy(buf, args ? args : L"", PATHMAX - 1); buf[PATHMAX - 1] = 0;
    wchar_t *ctx = NULL;
    for (wchar_t *t = wcstok(buf, L" ", &ctx); t; t = wcstok(NULL, L" ", &ctx)) {
        if (!wcscmp(t, L"--setup") || !wcscmp(t, L"--rebuild")) continue;
        if (wcslen(out) + wcslen(t) + 2 < cap) { wcscat(out, L" "); wcscat(out, t); }
    }
}

static int play(const wchar_t *args) {
    static wchar_t exe[PATHMAX], dir[PATHMAX], game[PATHMAX], user[PATHMAX], mod[PATHMAX], log[PATHMAX],
        cmd[4 * PATHMAX], extra[PATHMAX];
    swprintf(exe, PATHMAX, L"%ls\\runtime\\moderngekko-run.exe", root);
    swprintf(dir, PATHMAX, L"%ls\\runtime", root);
    path(game, L"game"); path(user, L"user"); path(mod, L"module\\gGMFE69_recomp.dll"); path(log, L"game.log");
    runtime_args(args, extra, PATHMAX);
    swprintf(cmd, sizeof cmd / sizeof *cmd,
        L"\"%ls\" --game \"%ls\" --user-dir \"%ls\" --module \"%ls\" --title \"Medal of Honor: Frontline\"%ls",
        exe, game, user, mod, extra);

    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE out = CreateFileW(log, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    STARTUPINFOW si = {0}; PROCESS_INFORMATION pi = {0}; si.cb = sizeof si;
    if (out != INVALID_HANDLE_VALUE && in != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = out; si.hStdError = out; si.hStdInput = in;
    }
    /* The runtime's DLLs (MSVC runtime, the time-zone shim) sit beside it, where Windows looks first. */
    BOOL ok = CreateProcessW(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, dir, &si, &pi);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
    if (!ok) { fail(L"The game could not start. Run MOHFrontline.exe --rebuild to rebuild it."); return 1; }
    place_on_fast_cores(pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    /* Launched from a launcher, the window can otherwise come up without keyboard focus. Shader
     * loading can take a while on the first start, so wait up to a minute for the window. */
    AllowSetForegroundWindow(pi.dwProcessId);
    g_pid = pi.dwProcessId;
    for (int i = 0; i < 600 && !g_window; i++) {
        if (WaitForSingleObject(pi.hProcess, 100) != WAIT_TIMEOUT) break;
        EnumWindows(find_window, 0);
    }
    if (g_window) { ShowWindow(g_window, SW_MAXIMIZE); SetForegroundWindow(g_window); }
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess);
    if (code == 0xC000041Du && clean_shutdown(log)) code = 0;
    if (code) {
        swprintf(cmd, PATHMAX, L"Medal of Honor: Frontline stopped with an error (exit code 0x%08lX). The details are in\n%ls\n\n"
                               L"If you moved your disc, run MOHFrontline.exe --setup to choose it again.", code, log);
        fail(cmd);
    }
    return (int)code;
}

int WINAPI wWinMain(HINSTANCE a, HINSTANCE b, PWSTR args, int show) {
    (void)a; (void)b; (void)show;
    if (!GetModuleFileNameW(NULL, root, PATHMAX)) return 1;
    wchar_t *slash = wcsrchr(root, L'\\'); if (!slash) return 1; *slash = 0;
    find_data();

    if (wcsstr(args, L"--rebuild")) { if (!run_setup(L"-Rebuild")) return 1; }
    else if (wcsstr(args, L"--setup")) {
        wchar_t p[PATHMAX]; path(p, L"disc.txt"); DeleteFileW(p);
        if (!run_setup(L"")) return 1;
    }
    else if (!built_for_this_kit()) { if (!run_setup(L"")) return 1; }
    return play(args);
}
