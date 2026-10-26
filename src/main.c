/* lineveil: a featherweight scanline overlay for the Windows desktop.
 *
 * Architecture
 *   - One hidden top-level "controller" window owns the tray icon, hotkey,
 *     and receives display-change broadcasts.
 *   - One click-through, topmost, layered overlay window per monitor.
 *   - The pattern is drawn once and handed to DWM. After that the process
 *     sits blocked in MsgWaitForMultipleObjects: zero CPU at idle.
 *   - Config file changes are watched with a directory change notification
 *     (no polling, no extra threads) and debounced with a timer.
 *
 * Build flags define UNICODE, WIN32_LEAN_AND_MEAN, _WIN32_WINNT=0x0A00.
 * Headers target Win10, but every post-Win7 API is either resolved at
 * runtime or fails harmlessly, so the binary runs on Windows 7 SP1+.
 */
#include <windows.h>
#include <shellapi.h>
#include "config.h"
#include "render.h"

#define APP_NAME        L"lineveil"
#define CTRL_CLASS      L"lineveil.controller"
#define OVERLAY_CLASS   L"lineveil.overlay"
#define MUTEX_NAME      L"Local\\lineveil.single-instance"
#define MAX_MONITORS    16

#define WM_TRAY         (WM_APP + 1)
#define WM_REBUILD_REQ  (WM_APP + 2)
#define ID_HOTKEY       1
#define ID_TIMER_DEBOUNCE 1
#define DEBOUNCE_MS     250

#define IDM_TOGGLE      100
#define IDM_CAPTURE     101
#define IDM_EDITCFG     102
#define IDM_RELOAD      103
#define IDM_EXIT        104

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

static HINSTANCE        g_inst;
static HWND             g_ctrl;
static HWND             g_overlays[MAX_MONITORS];
static int              g_overlay_count;
static Config           g_cfg;
static const Renderer  *g_renderer = &renderer_gdi;
static UINT             g_msg_taskbar_created;
static NOTIFYICONDATAW  g_nid;
static BOOL             g_hotkey_registered;
static BOOL             g_reload_pending;

/* ---------- overlays ---------- */

static void destroy_overlays(void) {
    int i;
    for (i = 0; i < g_overlay_count; i++) {
        g_renderer->release(g_overlays[i]);
        DestroyWindow(g_overlays[i]);
        g_overlays[i] = NULL;
    }
    g_overlay_count = 0;
}

static void apply_capture_affinity(void) {
    int i;
    DWORD mode = g_cfg.exclude_from_capture ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
    for (i = 0; i < g_overlay_count; i++)
        SetWindowDisplayAffinity(g_overlays[i], mode); /* fails quietly on old builds */
}

static BOOL CALLBACK monitor_proc(HMONITOR mon, HDC dc, LPRECT rc, LPARAM lp) {
    HWND hwnd;
    int w = rc->right - rc->left;
    int h = rc->bottom - rc->top;
    (void)mon; (void)dc; (void)lp;

    if (g_overlay_count >= MAX_MONITORS) return FALSE;

    hwnd = CreateWindowExW(
        g_renderer->ex_style | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        OVERLAY_CLASS, L"", WS_POPUP,
        rc->left, rc->top, w, h,
        NULL, NULL, g_inst, NULL);
    if (!hwnd) return TRUE;

    if (!g_renderer->apply(hwnd, w, h, &g_cfg)) {
        DestroyWindow(hwnd);
        return TRUE;
    }

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    g_overlays[g_overlay_count++] = hwnd;
    return TRUE;
}

static void build_overlays(void) {
    destroy_overlays();
    if (!g_cfg.enabled) return; /* fully torn down: DWM frees its surfaces too */
    EnumDisplayMonitors(NULL, NULL, monitor_proc, 0);
    apply_capture_affinity();
}

/* ---------- config / renderer / hotkey ---------- */

static void select_renderer(void) {
    if (g_cfg.renderer == RENDERER_DCOMP && renderer_dcomp.available())
        g_renderer = &renderer_dcomp;
    else
        g_renderer = &renderer_gdi;
}

static void register_hotkey(void) {
    if (g_hotkey_registered) {
        UnregisterHotKey(g_ctrl, ID_HOTKEY);
        g_hotkey_registered = FALSE;
    }
    g_hotkey_registered =
        RegisterHotKey(g_ctrl, ID_HOTKEY, g_cfg.hotkey_mods | MOD_NOREPEAT, g_cfg.hotkey_vk);
}

static void reload_all(void) {
    destroy_overlays();      /* with the renderer that created them */
    config_load(&g_cfg);
    select_renderer();
    register_hotkey();
    build_overlays();
}

/* ---------- tray ---------- */

static void tray_add(void) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_ctrl;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION); /* TODO: app icon resource */
    lstrcpynW(g_nid.szTip, APP_NAME, ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void tray_remove(void) {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

static void toggle(void) {
    g_cfg.enabled = !g_cfg.enabled; /* runtime only; the ini holds startup state */
    build_overlays();
}

static void tray_menu(void) {
    POINT pt;
    UINT cmd;
    HMENU m = CreatePopupMenu();
    if (!m) return;

    AppendMenuW(m, MF_STRING | (g_cfg.enabled ? MF_CHECKED : 0), IDM_TOGGLE, L"Scanlines on");
    AppendMenuW(m, MF_STRING | (g_cfg.exclude_from_capture ? MF_CHECKED : 0),
                IDM_CAPTURE, L"Hide from screen capture");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (config_path()[0] ? 0 : MF_GRAYED), IDM_EDITCFG, L"Edit config");
    AppendMenuW(m, MF_STRING, IDM_RELOAD, L"Reload config");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_EXIT, L"Exit");

    GetCursorPos(&pt);
    SetForegroundWindow(g_ctrl); /* required so the menu closes on outside click */
    cmd = (UINT)TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                               pt.x, pt.y, 0, g_ctrl, NULL);
    PostMessageW(g_ctrl, WM_NULL, 0, 0);
    DestroyMenu(m);

    switch (cmd) {
    case IDM_TOGGLE:  toggle(); break;
    case IDM_CAPTURE:
        g_cfg.exclude_from_capture = !g_cfg.exclude_from_capture;
        apply_capture_affinity();
        break;
    case IDM_EDITCFG:
        /* Path is ours (exe dir or %APPDATA%), never user-supplied input. */
        ShellExecuteW(NULL, L"open", config_path(), NULL, NULL, SW_SHOWNORMAL);
        break;
    case IDM_RELOAD:  reload_all(); break;
    case IDM_EXIT:    DestroyWindow(g_ctrl); break;
    }
}

/* ---------- window procs ---------- */

static void schedule_rebuild(BOOL reload_config) {
    if (reload_config) g_reload_pending = TRUE;
    SetTimer(g_ctrl, ID_TIMER_DEBOUNCE, DEBOUNCE_MS, NULL); /* resets if already set */
}

static LRESULT CALLBACK overlay_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_DPICHANGED:
    case WM_DISPLAYCHANGE:
        /* Per-monitor scale changes land here, not on the hidden controller. */
        PostMessageW(g_ctrl, WM_REBUILD_REQ, 0, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK ctrl_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_HOTKEY:
        if (wp == ID_HOTKEY) toggle();
        return 0;

    case WM_TRAY:
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) tray_menu();
        else if (lp == WM_LBUTTONDBLCLK) toggle();
        return 0;

    case WM_DISPLAYCHANGE:
    case WM_REBUILD_REQ:
        schedule_rebuild(FALSE);
        return 0;

    case WM_SETTINGCHANGE:
        if (wp == SPI_SETWORKAREA) schedule_rebuild(FALSE);
        return 0;

    case WM_TIMER:
        if (wp == ID_TIMER_DEBOUNCE) {
            KillTimer(hwnd, ID_TIMER_DEBOUNCE);
            if (g_reload_pending) {
                g_reload_pending = FALSE;
                reload_all();
            } else {
                build_overlays();
            }
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_DEBOUNCE);
        if (g_hotkey_registered) UnregisterHotKey(hwnd, ID_HOTKEY);
        destroy_overlays();
        tray_remove();
        PostQuitMessage(0);
        return 0;
    }

    if (msg == g_msg_taskbar_created && msg != 0) {
        tray_add(); /* Explorer restarted */
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ---------- DPI awareness ----------
 * Resolved at runtime so the exe still launches on Windows 7 / 8.1:
 *   Win10 1703+  per-monitor v2   (user32!SetProcessDpiAwarenessContext)
 *   Win8.1+      per-monitor v1   (shcore!SetProcessDpiAwareness)
 *   Win7         system aware     (user32!SetProcessDPIAware)
 */
typedef BOOL    (WINAPI *PFN_SetProcessDpiAwarenessContext)(HANDLE);
typedef HRESULT (WINAPI *PFN_SetProcessDpiAwareness)(int);
typedef BOOL    (WINAPI *PFN_SetProcessDPIAware)(void);

static void set_dpi_awareness(void) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    HMODULE shcore;

    if (user32) {
        PFN_SetProcessDpiAwarenessContext f = (PFN_SetProcessDpiAwarenessContext)
            (void *)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (f && f((HANDLE)-4 /* PER_MONITOR_AWARE_V2 */)) return;
    }

    /* shcore.dll only exists on 8.1+, where LOAD_LIBRARY_SEARCH_SYSTEM32 is
       always supported, so this can never be hijacked via the app directory. */
    shcore = LoadLibraryExW(L"shcore.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (shcore) {
        PFN_SetProcessDpiAwareness f = (PFN_SetProcessDpiAwareness)
            (void *)GetProcAddress(shcore, "SetProcessDpiAwareness");
        HRESULT hr = f ? f(2 /* PROCESS_PER_MONITOR_DPI_AWARE */) : E_FAIL;
        if (SUCCEEDED(hr)) return; /* keep shcore loaded; it's tiny */
        FreeLibrary(shcore);
    }

    if (user32) {
        PFN_SetProcessDPIAware f = (PFN_SetProcessDPIAware)
            (void *)GetProcAddress(user32, "SetProcessDPIAware");
        if (f) f();
    }
}

/* ---------- entry ---------- */

static int run(void) {
    WNDCLASSW wc;
    MSG msg;
    HANDLE mutex, watch = INVALID_HANDLE_VALUE;

    g_inst = GetModuleHandleW(NULL);

    mutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    /* Physical pixels on every monitor, so 1px lines stay 1px. */
    set_dpi_awareness();

    config_init();
    config_load(&g_cfg);
    select_renderer();

    ZeroMemory(&wc, sizeof(wc));
    wc.hInstance = g_inst;
    wc.lpfnWndProc = overlay_proc;
    wc.lpszClassName = OVERLAY_CLASS;
    if (!RegisterClassW(&wc)) return 1;

    wc.lpfnWndProc = ctrl_proc;
    wc.lpszClassName = CTRL_CLASS;
    if (!RegisterClassW(&wc)) return 1;

    g_msg_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

    /* Hidden top-level window (never shown). Not HWND_MESSAGE, because
       message-only windows don't receive WM_DISPLAYCHANGE broadcasts. */
    g_ctrl = CreateWindowExW(WS_EX_TOOLWINDOW, CTRL_CLASS, APP_NAME, WS_POPUP,
                             0, 0, 0, 0, NULL, NULL, g_inst, NULL);
    if (!g_ctrl) return 1;

    tray_add();
    register_hotkey();
    build_overlays();

    if (config_dir()[0])
        watch = FindFirstChangeNotificationW(config_dir(), FALSE,
                                             FILE_NOTIFY_CHANGE_LAST_WRITE);

    for (;;) {
        DWORD nh = (watch != INVALID_HANDLE_VALUE) ? 1 : 0;
        DWORD r = MsgWaitForMultipleObjects(nh, &watch, FALSE, INFINITE, QS_ALLINPUT);

        if (nh && r == WAIT_OBJECT_0) {
            if (!FindNextChangeNotification(watch)) {
                FindCloseChangeNotification(watch);
                watch = INVALID_HANDLE_VALUE;
            }
            /* The watch is per-directory; only react to our file. */
            if (config_changed()) schedule_rebuild(TRUE);
            continue;
        }

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

done:
    if (watch != INVALID_HANDLE_VALUE) FindCloseChangeNotification(watch);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return (int)msg.wParam;
}

/* No CRT: this is the process entry point (see build files).
   __cdecl so the x86 symbol is plain "_entry" for both MSVC and MinGW;
   the calling convention is irrelevant on x64 and ARM64. */
void __cdecl entry(void) {
    ExitProcess((UINT)run());
}
