/* Config handling.
 *
 * Parsing is delegated to the Win32 INI API (GetPrivateProfile*), so the only
 * hand-written parsing in this project is the 6-digit hex color. Every value is
 * clamped after reading. The config path is always absolute: GetPrivateProfile*
 * with a bare filename would silently fall back to %WINDIR%, so we never call it
 * without a resolved path.
 */
#include "config.h"

#define SECTION L"scanlite"
#define INI_NAME L"scanlite.ini"

static wchar_t g_path[MAX_PATH];
static wchar_t g_dir[MAX_PATH];
static BOOL g_have_path;
static FILETIME g_last_write;

static const char DEFAULT_INI[] =
    "; scanlite configuration\r\n"
    "; Saved changes are picked up automatically.\r\n"
    "\r\n"
    "[scanlite]\r\n"
    "; 1 = overlay on at startup\r\n"
    "enabled=1\r\n"
    "\r\n"
    "; Distance in pixels from one line to the next (2..64)\r\n"
    "period=3\r\n"
    "\r\n"
    "; Line thickness in pixels (1..period-1)\r\n"
    "thickness=1\r\n"
    "\r\n"
    "; Line opacity 0..255\r\n"
    "opacity=80\r\n"
    "\r\n"
    "; Line color as RRGGBB hex\r\n"
    "color=000000\r\n"
    "\r\n"
    "; horizontal or vertical\r\n"
    "orientation=horizontal\r\n"
    "\r\n"
    "; 1 = hide overlay from screenshots and screen sharing (Windows 10 2004+)\r\n"
    "exclude_from_capture=0\r\n"
    "\r\n"
    "; gdi (default) or dcomp (experimental, not implemented yet)\r\n"
    "renderer=gdi\r\n"
    "\r\n"
    "; Toggle hotkey. Modifiers: Alt=1 Ctrl=2 Shift=4 Win=8 (add them).\r\n"
    "; Key is a decimal virtual-key code. Default Ctrl+Alt+F12.\r\n"
    "hotkey_mods=3\r\n"
    "hotkey_vk=123\r\n";

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int hexval(wchar_t ch) {
    if (ch >= L'0' && ch <= L'9') return ch - L'0';
    if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
    return -1;
}

/* Accepts exactly "RRGGBB" or "#RRGGBB". Anything else returns fallback.
   Stops at the first non-hex char, so it never reads past the terminator. */
static DWORD parse_hex_rgb(const wchar_t *s, DWORD fallback) {
    DWORD v = 0;
    int i;
    if (*s == L'#') s++;
    for (i = 0; i < 6; i++) {
        int h = hexval(s[i]);
        if (h < 0) return fallback;
        v = (v << 4) | (DWORD)h;
    }
    return s[6] == 0 ? v : fallback;
}

/* dst = a + b. Returns FALSE if it would not fit. */
static BOOL join(wchar_t *dst, const wchar_t *a, const wchar_t *b) {
    if (lstrlenW(a) + lstrlenW(b) + 1 > MAX_PATH) return FALSE;
    lstrcpyW(dst, a);
    lstrcatW(dst, b);
    return TRUE;
}

static void write_default_file(const wchar_t *path) {
    DWORD written;
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    WriteFile(f, DEFAULT_INI, sizeof(DEFAULT_INI) - 1, &written, NULL);
    CloseHandle(f);
}

static BOOL get_write_time(FILETIME *ft) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!g_have_path || !GetFileAttributesExW(g_path, GetFileExInfoStandard, &d))
        return FALSE;
    *ft = d.ftLastWriteTime;
    return TRUE;
}

void config_init(void) {
    wchar_t exe[MAX_PATH], candidate[MAX_PATH];
    DWORD n;
    int i;

    g_have_path = FALSE;
    g_path[0] = g_dir[0] = 0;

    /* 1. Portable mode: scanlite.ini next to the exe. */
    n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        for (i = (int)n - 1; i >= 0 && exe[i] != L'\\'; i--) {}
        if (i > 0) {
            exe[i + 1] = 0; /* keep trailing backslash */
            if (join(candidate, exe, INI_NAME) &&
                GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES) {
                lstrcpyW(g_path, candidate);
                exe[i] = 0;
                lstrcpyW(g_dir, exe);
                g_have_path = TRUE;
            }
        }
    }

    /* 2. Per-user: %APPDATA%\scanlite\scanlite.ini */
    if (!g_have_path) {
        wchar_t appdata[MAX_PATH];
        n = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
        if (n > 0 && n < MAX_PATH && join(g_dir, appdata, L"\\scanlite")) {
            CreateDirectoryW(g_dir, NULL); /* ok if it already exists */
            if (join(g_path, g_dir, L"\\" INI_NAME)) {
                g_have_path = TRUE;
                if (GetFileAttributesW(g_path) == INVALID_FILE_ATTRIBUTES)
                    write_default_file(g_path);
            }
        }
    }

    if (!g_have_path) g_path[0] = g_dir[0] = 0;
    get_write_time(&g_last_write);
}

void config_load(Config *c) {
    wchar_t buf[32];

    /* Defaults */
    c->enabled = 1;
    c->period = 3;
    c->thickness = 1;
    c->opacity = 80;
    c->color = 0x000000;
    c->vertical = 0;
    c->exclude_from_capture = 0;
    c->renderer = RENDERER_GDI;
    c->hotkey_mods = MOD_CONTROL | MOD_ALT;
    c->hotkey_vk = VK_F12;

    if (!g_have_path) return;

    c->enabled   = GetPrivateProfileIntW(SECTION, L"enabled", c->enabled, g_path) != 0;
    c->period    = clampi((int)GetPrivateProfileIntW(SECTION, L"period", c->period, g_path), 2, 64);
    c->thickness = clampi((int)GetPrivateProfileIntW(SECTION, L"thickness", c->thickness, g_path), 1, c->period - 1);
    c->opacity   = clampi((int)GetPrivateProfileIntW(SECTION, L"opacity", c->opacity, g_path), 0, 255);

    GetPrivateProfileStringW(SECTION, L"color", L"000000", buf, 32, g_path);
    c->color = parse_hex_rgb(buf, 0x000000);

    GetPrivateProfileStringW(SECTION, L"orientation", L"horizontal", buf, 32, g_path);
    c->vertical = lstrcmpiW(buf, L"vertical") == 0;

    c->exclude_from_capture =
        GetPrivateProfileIntW(SECTION, L"exclude_from_capture", 0, g_path) != 0;

    GetPrivateProfileStringW(SECTION, L"renderer", L"gdi", buf, 32, g_path);
    c->renderer = lstrcmpiW(buf, L"dcomp") == 0 ? RENDERER_DCOMP : RENDERER_GDI;

    c->hotkey_mods = GetPrivateProfileIntW(SECTION, L"hotkey_mods", c->hotkey_mods, g_path) & 0xF;
    c->hotkey_vk   = (UINT)clampi((int)GetPrivateProfileIntW(SECTION, L"hotkey_vk", c->hotkey_vk, g_path), 1, 254);
}

const wchar_t *config_path(void) { return g_path; }
const wchar_t *config_dir(void) { return g_dir; }

BOOL config_changed(void) {
    FILETIME ft;
    if (!get_write_time(&ft)) return FALSE;
    if (CompareFileTime(&ft, &g_last_write) == 0) return FALSE;
    g_last_write = ft;
    return TRUE;
}
