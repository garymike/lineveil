/* Native unit tests for the line pattern and presets: make test */
#include <stdio.h>
#include <string.h>
#include "../src/config.h"

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } } while (0)

static Config pixel_cfg(int period, int thickness) {
    Config c; memset(&c, 0, sizeof c);
    c.period = period; c.thickness = thickness; c.fill = 50;
    return c;
}

static Config lines_cfg(int lines, int fill) {
    Config c = pixel_cfg(3, 1);
    c.lines = lines; c.fill = fill;
    return c;
}

/* Number of separate lines (runs of "on") across an extent. */
static int count_lines(const Config *c, int extent, int *on_rows, int *min_on, int *min_off) {
    int i, n = 0, run_on = 0, run_off = 0, prev = 0;
    *on_rows = 0; *min_on = 1 << 30; *min_off = 1 << 30;
    for (i = 0; i < extent; i++) {
        int on = pattern_on(c, extent, i);
        if (on) {
            (*on_rows)++;
            if (!prev) { n++; if (i > 0 && run_off < *min_off) *min_off = run_off; run_on = 0; }
            run_on++;
        } else {
            if (prev && run_on < *min_on) *min_on = run_on;
            if (prev) run_off = 0;
            run_off++;
        }
        prev = on;
    }
    if (prev && run_on < *min_on) *min_on = run_on;
    return n;
}

int main(void) {
    int on, mn_on, mn_off, n, e, lines, f, i;

    /* Pixel mode matches the original fixed-spacing formula exactly. */
    {
        int p, t;
        for (p = 2; p <= 64; p++) for (t = 1; t < p; t++) {
            Config c = pixel_cfg(p, t);
            for (i = 0; i < 300; i++)
                CHECK(pattern_on(&c, 1080, i) == (i % p < t), "pixel p=%d t=%d i=%d", p, t, i);
        }
    }

    /* Presets resolve to the documented line counts. */
    CHECK(preset_from_name(L"240p") == PRESET_240P, "240p name");
    CHECK(preset_from_name(L"240P") == PRESET_240P, "case-insensitive");
    CHECK(preset_from_name(L"224p") == PRESET_224P, "224p name");
    CHECK(preset_from_name(L"288p") == PRESET_288P, "288p name");
    CHECK(preset_from_name(L"480") == PRESET_480, "480 name");
    CHECK(preset_from_name(L"") == PRESET_NONE, "empty name");
    CHECK(preset_from_name(L"1080p") == PRESET_NONE, "unknown name");
    CHECK(preset_lines(PRESET_224P) == 224 && preset_lines(PRESET_240P) == 240 &&
          preset_lines(PRESET_288P) == 288 && preset_lines(PRESET_480) == 480, "preset line counts");
    CHECK(preset_lines(PRESET_NONE) == 0 && preset_lines(99) == 0 && preset_lines(-1) == 0, "out of range");
    CHECK(preset_name(PRESET_NONE) == NULL && preset_name(PRESET_COUNT) == NULL, "no name for none");
    {
        Config c = pixel_cfg(3, 1);
        config_set_preset(&c, PRESET_288P);
        CHECK(c.preset == PRESET_288P && c.lines == 288, "set preset");
        config_set_preset(&c, PRESET_NONE);
        CHECK(c.preset == PRESET_NONE && c.lines == 0, "back to pixel mode");
    }

    /* The core promise: the same number of lines on every monitor. */
    {
        static const int heights[] = { 720, 768, 900, 1050, 1080, 1200, 1440, 1600, 2160, 2880, 4320 };
        for (i = 0; i < (int)(sizeof heights / sizeof heights[0]); i++) {
            Config c = lines_cfg(240, 50);
            n = count_lines(&c, heights[i], &on, &mn_on, &mn_off);
            CHECK(n == 240, "240p on %d px gave %d lines", heights[i], n);
            CHECK(mn_on >= 1 && mn_off >= 1, "240p on %d px: line or gap vanished", heights[i]);
        }
    }

    /* Integer spacing is perfectly regular: 240 lines on 1440 px = 6 px, 3 dark. */
    {
        Config c = lines_cfg(240, 50);
        for (i = 0; i < 1440; i++)
            CHECK(pattern_on(&c, 1440, i) == (i % 6 < 3), "1440 regular i=%d", i);
    }

    /* Non-integer spacing alternates evenly: 240 on 1080 = 4.5 px cycles. */
    {
        Config c = lines_cfg(240, 50);
        n = count_lines(&c, 1080, &on, &mn_on, &mn_off);
        /* 2.25 dark px per cycle can't land on whole pixels: cycles alternate
           2-on/2-off and 3-on/2-off, so expect 540 rows give or take a
           quarter row per line. */
        CHECK(on >= 480 && on <= 600, "240 on 1080 at 50%% should darken ~540 rows, got %d", on);
        CHECK(mn_on == 2 && mn_off == 2, "4.5px cycles should be 2-3 px each, got on>=%d off>=%d", mn_on, mn_off);
    }

    /* Too many lines for the screen clamps to 2 px cycles instead of merging. */
    {
        Config c = lines_cfg(2160, 50);
        n = count_lines(&c, 1080, &on, &mn_on, &mn_off);
        CHECK(n == 540, "2160 lines on 1080 px should clamp to 540, got %d", n);
    }

    /* Broad sweep: exact line count, never a vanished line or gap, fill honored. */
    for (e = 100; e <= 4320; e += 37) {
        for (lines = 16; lines <= 2160; lines += 29) {
            for (f = 10; f <= 90; f += 20) {
                Config c = lines_cfg(lines, f);
                int want = lines > e / 2 ? e / 2 : lines;
                n = count_lines(&c, e, &on, &mn_on, &mn_off);
                CHECK(n == want, "e=%d lines=%d fill=%d: %d lines, want %d", e, lines, f, n, want);
                CHECK(mn_on >= 1 && (want == 1 || mn_off >= 1),
                      "e=%d lines=%d fill=%d: min on %d, min off %d", e, lines, f, mn_on, mn_off);
                /* Dark rows track fill, within one row per line (or the 1 px clamps). */
                {
                    int lo = e * f / 100 - want, hi = e * f / 100 + want;
                    if (want * 1 > lo) lo = 0; /* clamp to at least 1 px per line may raise it */
                    CHECK(on >= lo && on <= hi + want, "e=%d lines=%d fill=%d: %d dark rows", e, lines, f, on);
                }
                if (failures > 20) goto done;
            }
        }
    }

    /* Out-of-range indices are never "on". */
    {
        Config c = lines_cfg(240, 50);
        CHECK(!pattern_on(&c, 1080, -1) && !pattern_on(&c, 1080, 1080) && !pattern_on(&c, 0, 0), "bounds");
    }

done:
    printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
