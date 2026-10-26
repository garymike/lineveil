/* Line pattern and presets.
 *
 * Two modes:
 *   Pixel mode  (lines == 0): a line every `period` px, `thickness` px thick.
 *   Lines mode  (lines  > 0): exactly `lines` lines across the monitor, so a
 *                240p preset shows 240 lines on 1080p, 1440p and 4K alike.
 *
 * Lines mode samples each pixel at its center and asks where it falls inside
 * its line cycle. With a non-integer spacing (240 lines on 1080 px = 4.5 px)
 * cycles alternate between 4 and 5 px, which is how real low-res sources
 * looked when shown on a fixed pixel grid.
 *
 * All math is 32-bit unsigned on clamped inputs, so it never overflows and
 * never needs 64-bit runtime helpers on 32-bit MSVC builds.
 */
#include "config.h"

#define MAX_EXTENT 32767u /* far beyond any real monitor; keeps products in 32 bits */

typedef struct { const wchar_t *name; int lines; } Preset;

/* Line counts are the active lines per frame (per field for interlaced
   systems), i.e. what a low-res game or computer actually drew. */
static const Preset PRESETS[PRESET_COUNT] = {
    { 0,       0   },
    { L"224p", 224 }, /* most arcade boards, much of SNES */
    { L"240p", 240 }, /* NES, Genesis, PS1 and other NTSC consoles */
    { L"288p", 288 }, /* PAL consoles */
    { L"480",  480 }, /* VGA 640x480, 480i/480p video */
};

const wchar_t *preset_name(int p) {
    return (p > PRESET_NONE && p < PRESET_COUNT) ? PRESETS[p].name : 0;
}

int preset_lines(int p) {
    return (p > PRESET_NONE && p < PRESET_COUNT) ? PRESETS[p].lines : 0;
}

int preset_from_name(const wchar_t *s) {
    int p;
    for (p = PRESET_NONE + 1; p < PRESET_COUNT; p++)
        if (lstrcmpiW(s, PRESETS[p].name) == 0) return p;
    return PRESET_NONE;
}

void config_set_preset(Config *c, int p) {
    if (p > PRESET_NONE && p < PRESET_COUNT) {
        c->preset = p;
        c->lines = PRESETS[p].lines;
    } else {
        c->preset = PRESET_NONE;
        c->lines = 0;
    }
}

BOOL pattern_on(const Config *c, int extent, int i) {
    unsigned e, n, phase2, threshold, lo, hi;

    if (extent <= 0 || i < 0 || i >= extent) return FALSE;

    if (c->lines <= 0)
        return (i % c->period) < c->thickness;

    e = (unsigned)extent;
    if (e > MAX_EXTENT) e = MAX_EXTENT;
    n = (unsigned)c->lines;
    if (n > e / 2) n = e / 2;   /* lines need at least 2 px per cycle */
    if (n < 1) n = 1;

    /* Phase of the pixel center within its cycle, scaled to [0, 2e). */
    phase2 = ((2u * (unsigned)i + 1u) * n) % (2u * e);

    /* Dark portion of the cycle, kept to at least 1 px dark and 1 px clear
       per cycle so every line stays visible and lines never merge. */
    threshold = 2u * e * (unsigned)c->fill / 100u;
    lo = 2u * n;
    hi = 2u * e - 2u * n;
    if (threshold < lo) threshold = lo;
    if (threshold > hi) threshold = hi;

    return phase2 < threshold;
}
