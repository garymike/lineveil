#ifndef LINEVEIL_CONFIG_H
#define LINEVEIL_CONFIG_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { RENDERER_GDI = 0, RENDERER_DCOMP = 1 };

/* Built-in presets: line counts from real video standards. Index 0 = none. */
enum { PRESET_NONE = 0, PRESET_224P, PRESET_240P, PRESET_288P, PRESET_480, PRESET_COUNT };

typedef struct Config {
    int   enabled;              /* overlay on at startup */
    /* Pixel mode (lines == 0): fixed spacing in physical pixels. */
    int   period;               /* pixels from one line start to the next */
    int   thickness;            /* line thickness in pixels (< period) */
    /* Lines mode (lines > 0): a fixed number of lines across each monitor,
       so every display shows the same count whatever its resolution. */
    int   preset;               /* PRESET_*, PRESET_NONE when custom/pixel mode */
    int   lines;                /* lines per screen (height, or width if vertical) */
    int   fill;                 /* percent of each line cycle that is dark, 10..90 */
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

/* Preset metadata. Names are the config spelling ("240p"). */
const wchar_t *preset_name(int preset);   /* NULL for PRESET_NONE / out of range */
int preset_lines(int preset);             /* 0 for PRESET_NONE */
int preset_from_name(const wchar_t *s);   /* PRESET_NONE if unknown */

/* Applies a preset to c (sets preset + lines). PRESET_NONE switches back to
   pixel mode. */
void config_set_preset(Config *c, int preset);

/* TRUE if pixel `i` along an axis of `extent` pixels is part of a line.
   Shared by both renderers so they always draw the same pattern. */
BOOL pattern_on(const Config *c, int extent, int i);

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
