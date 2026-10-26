#ifndef SCANLITE_CONFIG_H
#define SCANLITE_CONFIG_H

#include <windows.h>

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
    UINT  hotkey_mods;          /* MOD_ALT=1, MOD_CONTROL=2, MOD_SHIFT=4, MOD_WIN=8 */
    UINT  hotkey_vk;            /* virtual-key code, decimal (F12 = 123) */
} Config;

/* Resolves the config path (portable next to exe, else %APPDATA%\scanlite)
   and writes a default file if none exists. */
void config_init(void);

/* Loads and clamps all values. Falls back to defaults if no path is available. */
void config_load(Config *c);

const wchar_t *config_path(void);   /* empty string if unavailable */
const wchar_t *config_dir(void);    /* empty string if unavailable */

/* Returns TRUE if the config file's last-write time changed since the last call. */
BOOL config_changed(void);

#endif
