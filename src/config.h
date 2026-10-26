#ifndef LINEVEIL_CONFIG_H
#define LINEVEIL_CONFIG_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { RENDERER_GDI = 0, RENDERER_DCOMP = 1 };

typedef struct Config {
    int   enabled;              /* overlay on at startup */
    int   period;               /* pixels from one line start to the next */
    int   thickness;            /* line thickness in pixels (< period) */
    int   opacity;              /* 0..255 */
    DWORD color;                /* 0xRRGGBB */
    int   vertical;             /* 0 = horizontal lines, 1 = vertical */
    int   exclude_from_capture; /* hide overlay from screenshots/screen share */
    int   renderer;             /* RENDERER_GDI or RENDERER_DCOMP */
    UINT  hotkey_mods;          /* MOD_* bits; 0 with hotkey_vk 0 = no hotkey */
    UINT  hotkey_vk;            /* virtual-key code */
    wchar_t hotkey_text[40];    /* normalized, e.g. "Ctrl+Alt+F12", for display */
} Config;

/* Parses "Ctrl+Alt+F12" style text. Returns FALSE (outputs untouched) if invalid.
   "none" parses to mods=0, vk=0. Exposed for testing. */
BOOL hotkey_parse(const wchar_t *text, UINT *mods, UINT *vk);

/* Writes a normalized name like "Ctrl+Shift+F9" into out (cap chars). */
void hotkey_format(UINT mods, UINT vk, wchar_t *out, int cap);

/* Resolves the config path (portable next to exe, else %APPDATA%\lineveil)
   and writes a default file if none exists. */
void config_init(void);

/* Loads and clamps all values. Falls back to defaults if no path is available. */
void config_load(Config *c);

const wchar_t *config_path(void);   /* empty string if unavailable */
const wchar_t *config_dir(void);    /* empty string if unavailable */

/* Returns TRUE if the config file's last-write time changed since the last call. */
BOOL config_changed(void);

#ifdef __cplusplus
}
#endif

#endif
