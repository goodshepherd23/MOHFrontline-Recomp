/* MOHFrontline.exe -- launcher for the Medal of Honor: Frontline (Recompiled) kit.
 *
 * Player data (the game files, the built module, settings and saves) lives outside the app folder,
 * so a launcher update that replaces the app folder cannot wipe it. The first run asks where to put
 * it (default %LOCALAPPDATA%\MOHFrontline-Recomp); the choice is remembered in
 * %LOCALAPPDATA%\MOHFrontline-Recomp\location.txt. A file named portable.txt beside
 * MOHFrontline.exe keeps everything in the app folder instead.
 *
 * 1. Runs kit\setup.ps1 when the game has not been built for this kit's build inputs yet (it asks
 *    for the disc the first time).
 * 2. Shows a settings window (resolution, graphics, fullscreen, widescreen, controls), unless the
 *    player turned it off; holding Shift while starting shows it again.
 * 3. Starts runtime\moderngekko-run.exe with the built module, maximized and focused, on the fastest
 *    CPU cores. Other arguments are passed to the runtime unchanged.
 *
 * Usage: MOHFrontline.exe            play (sets up first if needed)
 *        MOHFrontline.exe --setup    choose another disc
 *        MOHFrontline.exe --rebuild  rebuild the game from the disc
 * GPL-3.0, see LICENSE. */
#define _WIN32_WINNT 0x0A00
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
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
static int is_dir(const wchar_t *p) { DWORD a = GetFileAttributesW(p); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }

static void note(const wchar_t *msg) {   /* appends to launcher.log in the data folder */
    wchar_t p[PATHMAX]; path(p, L"launcher.log");
    FILE *f = _wfopen(p, L"a, ccs=UTF-8"); if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fwprintf(f, L"%04u-%02u-%02u %02u:%02u:%02u %ls\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, msg);
    fclose(f);
}
static void fail(const wchar_t *msg) { note(msg); MessageBoxW(NULL, msg, TITLE, MB_ICONERROR); }

/* Reads the first line of a small text file into out (trimmed); returns 0 if missing. */
static int read_line(const wchar_t *file, wchar_t *out, size_t cap) {
    FILE *f = _wfopen(file, L"r, ccs=UTF-8");
    if (!f) return 0;
    out[0] = 0;
    if (!fgetws(out, (int)cap, f)) out[0] = 0;
    fclose(f);
    size_t n = wcslen(out);
    while (n && (out[n - 1] == L'\n' || out[n - 1] == L'\r' || out[n - 1] == L' ')) out[--n] = 0;
    return 1;
}
static void write_line(const wchar_t *file, const wchar_t *text) {
    FILE *f = _wfopen(file, L"w, ccs=UTF-8"); if (!f) return;
    fwprintf(f, L"%ls\n", text); fclose(f);
}

/* ------------------------------------------------------------------ install folder */

static int pick_folder(wchar_t *out) {
    IFileOpenDialog *dlg = NULL; int ok = 0;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&dlg)))
        return 0;
    DWORD opts = 0;
    IFileOpenDialog_GetOptions(dlg, &opts);
    IFileOpenDialog_SetOptions(dlg, opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    IFileOpenDialog_SetTitle(dlg, L"Choose where to install Medal of Honor: Frontline (about 3 GB)");
    if (SUCCEEDED(IFileOpenDialog_Show(dlg, NULL))) {
        IShellItem *item = NULL;
        if (SUCCEEDED(IFileOpenDialog_GetResult(dlg, &item))) {
            PWSTR p = NULL;
            if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &p))) {
                wcsncpy(out, p, PATHMAX - 1); out[PATHMAX - 1] = 0; ok = 1; CoTaskMemFree(p);
            }
            IShellItem_Release(item);
        }
    }
    IFileOpenDialog_Release(dlg);
    return ok;
}

/* Returns 0 if the player cancelled the first-run choice. */
static int find_data(void) {
    wchar_t p[PATHMAX], local[PATHMAX], home[PATHMAX], pointer[PATHMAX], saved[PATHMAX];
    swprintf(p, PATHMAX, L"%ls\\portable.txt", root);
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, PATHMAX);
    if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES || !n || n >= PATHMAX) { wcscpy(data, root); return 1; }
    swprintf(home, PATHMAX, L"%ls\\MOHFrontline-Recomp", local);
    swprintf(pointer, PATHMAX, L"%ls\\location.txt", home);
    if (read_line(pointer, saved, PATHMAX) && saved[0] && (is_dir(saved) || CreateDirectoryW(saved, NULL))) {
        wcscpy(data, saved); return 1;
    }
    /* An install from before the folder choice existed keeps its place. */
    wchar_t probe[PATHMAX];
    swprintf(probe, PATHMAX, L"%ls\\module", home);
    if (is_dir(probe)) { wcscpy(data, home); return 1; }

    swprintf(p, PATHMAX,
        L"Medal of Honor: Frontline needs about 3 GB for the game files and its build.\n\n"
        L"Install it here?\n%ls\n\nYes: install here\nNo: choose another folder", home);
    int answer = MessageBoxW(NULL, p, TITLE, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (answer == IDCANCEL) return 0;
    CreateDirectoryW(home, NULL);
    if (answer == IDNO) {
        wchar_t chosen[PATHMAX];
        if (!pick_folder(chosen)) return 0;
        size_t len = wcslen(chosen);
        if (len && chosen[len - 1] == L'\\') chosen[len - 1] = 0;
        swprintf(data, PATHMAX, L"%ls\\MOHFrontline-Recomp", chosen);
        if (!is_dir(data) && !CreateDirectoryW(data, NULL)) {
            swprintf(p, PATHMAX, L"Could not create\n%ls", data); fail(p); return 0;
        }
        write_line(pointer, data);
        return 1;
    }
    wcscpy(data, home);
    write_line(pointer, data);
    return 1;
}

/* module\.kit-build holds the build version setup built for (kit\build-version.txt). Kit updates
 * that do not change the build keep it, so they do not cost a rebuild. */
static int built_for_this_kit(void) {
    wchar_t p[PATHMAX], want[64] = L"", have[64] = L"";
    swprintf(p, PATHMAX, L"%ls\\kit\\build-version.txt", root);
    if (!read_line(p, want, 64) || !want[0]) wcscpy(want, KIT_VERSION);
    path(p, L"module\\.kit-build");
    if (!read_line(p, have, 64) || wcscmp(have, want)) return 0;
    return exists(L"module\\gGMFE69_recomp.dll") && exists(L"game\\sys\\main.dol") &&
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

/* Copies kit\user files that are missing from the data folder's user\ (never overwrites), so
 * defaults added by a kit update -- like a new per-game INI -- reach installs that do not rebuild. */
static void add_missing_defaults(const wchar_t *src, const wchar_t *dst) {
    wchar_t pattern[PATHMAX], s[PATHMAX], d[PATHMAX];
    swprintf(pattern, PATHMAX, L"%ls\\*", src);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    CreateDirectoryW(dst, NULL);
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        swprintf(s, PATHMAX, L"%ls\\%ls", src, fd.cFileName);
        swprintf(d, PATHMAX, L"%ls\\%ls", dst, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) add_missing_defaults(s, d);
        else CopyFileW(s, d, TRUE);   /* TRUE: keep the player's file if it exists */
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

/* ------------------------------------------------------------------ settings window */

enum { ID_RES = 100, ID_BACKEND, ID_FULLSCREEN, ID_WIDE, ID_FPS, ID_SKIP, ID_PLAY, ID_QUIT, ID_DISC,
       ID_REBUILD, ID_OPEN, ID_RESETPAD };
enum { CHOICE_QUIT, CHOICE_PLAY, CHOICE_DISC, CHOICE_REBUILD };

static const wchar_t *res_values[] = { L"640x528", L"1280x1056", L"1920x1584", L"2560x2112" };
static const wchar_t *res_names[] = { L"Native (640 x 528), fastest", L"2x (1280 x 1056)", L"3x (1920 x 1584)", L"4x (2560 x 2112)" };
static const wchar_t *backend_values[] = { L"D3D11", L"Vulkan", L"OpenGL" };
static const wchar_t *backend_names[] = { L"Direct3D 11 (recommended)", L"Vulkan", L"OpenGL" };

static wchar_t cfg_ini[PATHMAX], gfx_ini[PATHMAX], dolphin_ini[PATHMAX], launcher_ini[PATHMAX];
static int g_choice = CHOICE_QUIT;
static HFONT g_font;
static int g_dpi = 96;
static int S(int v) { return MulDiv(v, g_dpi, 96); }

static void ini_get(const wchar_t *file, const wchar_t *sec, const wchar_t *key, const wchar_t *def, wchar_t *out, DWORD cap) {
    GetPrivateProfileStringW(sec, key, def, out, cap, file);
}
static int ini_bool(const wchar_t *file, const wchar_t *sec, const wchar_t *key, int def) {
    wchar_t v[16]; ini_get(file, sec, key, def ? L"true" : L"false", v, 16);
    return !_wcsicmp(v, L"true") || !wcscmp(v, L"1");
}

static HWND add(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h), parent,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}
static void check(HWND w, int id, int on) { SendDlgItemMessageW(w, id, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0); }
static int checked(HWND w, int id) { return SendDlgItemMessageW(w, id, BM_GETCHECK, 0, 0) == BST_CHECKED; }

static void load_settings(HWND w) {
    wchar_t v[64];
    ini_get(cfg_ini, L"Video", L"resolution", L"640x528", v, 64);
    int r = 0; for (int i = 0; i < 4; i++) if (!_wcsicmp(v, res_values[i])) r = i;
    SendDlgItemMessageW(w, ID_RES, CB_SETCURSEL, r, 0);
    ini_get(cfg_ini, L"Video", L"backend", L"D3D11", v, 64);
    int b = 0; for (int i = 0; i < 3; i++) if (!_wcsicmp(v, backend_values[i])) b = i;
    SendDlgItemMessageW(w, ID_BACKEND, CB_SETCURSEL, b, 0);
    check(w, ID_FULLSCREEN, ini_bool(cfg_ini, L"Video", L"fullscreen", 0));
    check(w, ID_FPS, ini_bool(cfg_ini, L"Video", L"show_fps_in_title", 1));
    ini_get(gfx_ini, L"Settings", L"AspectRatio", L"1", v, 64);
    check(w, ID_WIDE, wcscmp(v, L"0") != 0 && ini_bool(gfx_ini, L"Settings", L"wideScreenHack", 1));
    check(w, ID_SKIP, ini_bool(launcher_ini, L"Launcher", L"SkipSettings", 0));
}

static void save_settings(HWND w) {
    int r = (int)SendDlgItemMessageW(w, ID_RES, CB_GETCURSEL, 0, 0);
    int b = (int)SendDlgItemMessageW(w, ID_BACKEND, CB_GETCURSEL, 0, 0);
    WritePrivateProfileStringW(L"Video", L"resolution", res_values[r < 0 ? 0 : r], cfg_ini);
    WritePrivateProfileStringW(L"Video", L"backend", backend_values[b < 0 ? 0 : b], cfg_ini);
    WritePrivateProfileStringW(L"Video", L"fullscreen", checked(w, ID_FULLSCREEN) ? L"true" : L"false", cfg_ini);
    WritePrivateProfileStringW(L"Video", L"show_fps_in_title", checked(w, ID_FPS) ? L"true" : L"false", cfg_ini);
    int wide = checked(w, ID_WIDE);
    WritePrivateProfileStringW(L"Settings", L"AspectRatio", wide ? L"1" : L"0", gfx_ini);
    WritePrivateProfileStringW(L"Settings", L"wideScreenHack", wide ? L"True" : L"False", gfx_ini);
    WritePrivateProfileStringW(L"Launcher", L"SkipSettings", checked(w, ID_SKIP) ? L"1" : L"0", launcher_ini);
}

/* Puts the kit's default controls back (controller + keyboard and mouse) and locks the mouse
 * cursor to the game window, which mouse look needs. */
static void reset_controls(HWND w) {
    wchar_t src[PATHMAX], dst[PATHMAX];
    swprintf(src, PATHMAX, L"%ls\\kit\\user\\Config\\GCPadNew.ini", root);
    path(dst, L"user\\Config\\GCPadNew.ini");
    if (CopyFileW(src, dst, FALSE)) {
        WritePrivateProfileStringW(L"Interface", L"LockCursor", L"True", dolphin_ini);
        MessageBoxW(w, L"The controls are back to the kit's defaults (see the README for the layout).", TITLE, MB_ICONINFORMATION);
    } else {
        MessageBoxW(w, L"Could not reset the controls.", TITLE, MB_ICONERROR);
    }
}

static LRESULT CALLBACK settings_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_PLAY: save_settings(w); g_choice = CHOICE_PLAY; DestroyWindow(w); return 0;
        case ID_QUIT: case IDCANCEL: g_choice = CHOICE_QUIT; DestroyWindow(w); return 0;
        case ID_DISC: save_settings(w); g_choice = CHOICE_DISC; DestroyWindow(w); return 0;
        case ID_REBUILD:
            if (MessageBoxW(w, L"Rebuild the game from your disc? This takes as long as the first setup.", TITLE,
                            MB_YESNO | MB_ICONQUESTION) == IDYES) { save_settings(w); g_choice = CHOICE_REBUILD; DestroyWindow(w); }
            return 0;
        case ID_OPEN: ShellExecuteW(w, L"open", data, NULL, NULL, SW_SHOWNORMAL); return 0;
        case ID_RESETPAD: reset_controls(w); return 0;
        }
        break;
    case WM_CLOSE: g_choice = CHOICE_QUIT; DestroyWindow(w); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

static int settings_window(void) {
    NONCLIENTMETRICSW ncm; memset(&ncm, 0, sizeof ncm); ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g_dpi = (int)GetDpiForSystem();
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = settings_proc; wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"MOHFrontlineSettings";
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassW(&wc);

    RECT r = { 0, 0, S(460), S(372) };
    AdjustWindowRect(&r, WS_CAPTION | WS_SYSMENU, FALSE);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    HWND w = CreateWindowExW(0, wc.lpszClassName, TITLE, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        (GetSystemMetrics(SM_CXSCREEN) - ww) / 2, (GetSystemMetrics(SM_CYSCREEN) - wh) / 2, ww, wh,
        NULL, NULL, wc.hInstance, NULL);

    wchar_t where[PATHMAX + 32];
    swprintf(where, sizeof where / sizeof *where, L"Game files: %ls", data);
    add(w, L"STATIC", where, SS_PATHELLIPSIS | SS_NOPREFIX, 16, 14, 340, 20, -1);
    add(w, L"BUTTON", L"Open folder", BS_PUSHBUTTON | WS_TABSTOP, 364, 10, 80, 26, ID_OPEN);

    add(w, L"BUTTON", L"Video", BS_GROUPBOX, 12, 44, 436, 172, -1);
    add(w, L"STATIC", L"Resolution:", 0, 28, 72, 110, 20, -1);
    HWND res = add(w, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 140, 68, 292, 200, ID_RES);
    for (int i = 0; i < 4; i++) SendMessageW(res, CB_ADDSTRING, 0, (LPARAM)res_names[i]);
    add(w, L"STATIC", L"Graphics:", 0, 28, 106, 110, 20, -1);
    HWND be = add(w, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 140, 102, 292, 200, ID_BACKEND);
    for (int i = 0; i < 3; i++) SendMessageW(be, CB_ADDSTRING, 0, (LPARAM)backend_names[i]);
    add(w, L"BUTTON", L"Fullscreen", BS_AUTOCHECKBOX | WS_TABSTOP, 28, 138, 200, 22, ID_FULLSCREEN);
    add(w, L"BUTTON", L"Widescreen 16:9 (menus are stretched)", BS_AUTOCHECKBOX | WS_TABSTOP, 28, 162, 400, 22, ID_WIDE);
    add(w, L"BUTTON", L"Show speed in the title bar", BS_AUTOCHECKBOX | WS_TABSTOP, 28, 186, 400, 22, ID_FPS);

    add(w, L"BUTTON", L"Controls", BS_GROUPBOX, 12, 224, 436, 62, -1);
    add(w, L"STATIC", L"Controller, or keyboard and mouse (WASD, mouse look, click to fire). See the README.",
        0, 28, 244, 300, 36, -1);
    add(w, L"BUTTON", L"Reset controls", BS_PUSHBUTTON | WS_TABSTOP, 336, 248, 100, 26, ID_RESETPAD);

    add(w, L"BUTTON", L"Don't show this window (hold Shift when starting to see it again)", BS_AUTOCHECKBOX | WS_TABSTOP,
        16, 294, 430, 22, ID_SKIP);
    add(w, L"BUTTON", L"Change disc...", BS_PUSHBUTTON | WS_TABSTOP, 12, 330, 100, 30, ID_DISC);
    add(w, L"BUTTON", L"Rebuild...", BS_PUSHBUTTON | WS_TABSTOP, 118, 330, 84, 30, ID_REBUILD);
    add(w, L"BUTTON", L"Quit", BS_PUSHBUTTON | WS_TABSTOP, 268, 330, 84, 30, ID_QUIT);
    HWND play = add(w, L"BUTTON", L"Play", BS_DEFPUSHBUTTON | WS_TABSTOP, 358, 330, 90, 30, ID_PLAY);

    load_settings(w);
    ShowWindow(w, SW_SHOWNORMAL);
    SetForegroundWindow(w);
    SetFocus(play);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (m.message == WM_KEYDOWN && m.wParam == VK_RETURN && IsWindow(w) && GetFocus() &&
            GetParent(GetFocus()) == w && GetDlgCtrlID(GetFocus()) != ID_RES && GetDlgCtrlID(GetFocus()) != ID_BACKEND) {
            int id = GetDlgCtrlID(GetFocus());
            SendMessageW(w, WM_COMMAND, (id == ID_QUIT || id == ID_DISC || id == ID_REBUILD || id == ID_OPEN || id == ID_RESETPAD) ? id : ID_PLAY, 0);
            continue;
        }
        if (IsWindow(w) && IsDialogMessageW(w, &m)) continue;
        TranslateMessage(&m); DispatchMessageW(&m);
    }
    DeleteObject(g_font);
    return g_choice;
}

/* ------------------------------------------------------------------ game */

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
                               L"If you moved your disc, choose it again with Change disc in the launcher.", code, log);
        fail(cmd);
    }
    return (int)code;
}

int WINAPI wWinMain(HINSTANCE a, HINSTANCE b, PWSTR args, int show) {
    (void)a; (void)b; (void)show;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (!GetModuleFileNameW(NULL, root, PATHMAX)) return 1;
    wchar_t *slash = wcsrchr(root, L'\\'); if (!slash) return 1; *slash = 0;
    if (!find_data()) return 0;
    path(cfg_ini, L"user\\config.ini"); path(gfx_ini, L"user\\Config\\GFX.ini");
    path(dolphin_ini, L"user\\Config\\Dolphin.ini"); path(launcher_ini, L"launcher.ini");

    int shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (wcsstr(args, L"--rebuild")) { if (!run_setup(L"-Rebuild")) return 1; }
    else if (wcsstr(args, L"--setup")) {
        wchar_t p[PATHMAX]; path(p, L"disc.txt"); DeleteFileW(p);
        if (!run_setup(L"")) return 1;
    }
    else if (!built_for_this_kit()) { if (!run_setup(L"")) return 1; }
    {
        wchar_t src[PATHMAX], dst[PATHMAX];
        swprintf(src, PATHMAX, L"%ls\\kit\\user", root);
        path(dst, L"user");
        add_missing_defaults(src, dst);
    }

    for (;;) {
        if (!shift && ini_bool(launcher_ini, L"Launcher", L"SkipSettings", 0)) break;
        int choice = settings_window();
        if (choice == CHOICE_QUIT) return 0;
        if (choice == CHOICE_PLAY) break;
        if (choice == CHOICE_DISC) {
            wchar_t p[PATHMAX]; path(p, L"disc.txt"); DeleteFileW(p);
            run_setup(L"");
        } else if (choice == CHOICE_REBUILD) {
            run_setup(L"-Rebuild");
        }
        shift = 1;   /* back to the window after a disc change or rebuild */
    }
    if (!built_for_this_kit()) { fail(L"The game is not built yet. Choose your disc with Change disc."); return 1; }
    return play(args);
}
