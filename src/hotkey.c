/* Hotkey text <-> (modifiers, virtual key).
 *
 * Accepts things like "Ctrl+Alt+F12", "shift + win + s", "None".
 * Rules:
 *   - tokens separated by '+', surrounding spaces ignored, case-insensitive
 *   - modifiers: Ctrl/Control, Alt, Shift, Win/Windows
 *   - exactly one key token; each modifier at most once
 *   - at least one modifier, except for keys nobody types alone
 *     (F13-F24, Pause, ScrollLock) so a bare letter can't hijack typing
 * Every buffer is bounded; anything unexpected is rejected, never truncated.
 */
#include "config.h"

#define MAX_TOKENS 6
#define MAX_TOKEN  16

typedef struct { const wchar_t *name; UINT vk; } KeyName;

/* First entry for a given vk is its canonical display name. */
static const KeyName KEYS[] = {
    { L"Space", VK_SPACE },       { L"Tab", VK_TAB },
    { L"Enter", VK_RETURN },      { L"Backspace", VK_BACK },
    { L"Esc", VK_ESCAPE },        { L"Escape", VK_ESCAPE },
    { L"Insert", VK_INSERT },     { L"Ins", VK_INSERT },
    { L"Delete", VK_DELETE },     { L"Del", VK_DELETE },
    { L"Home", VK_HOME },         { L"End", VK_END },
    { L"PageUp", VK_PRIOR },      { L"PgUp", VK_PRIOR },
    { L"PageDown", VK_NEXT },     { L"PgDn", VK_NEXT },
    { L"Up", VK_UP },             { L"Down", VK_DOWN },
    { L"Left", VK_LEFT },         { L"Right", VK_RIGHT },
    { L"Pause", VK_PAUSE },       { L"ScrollLock", VK_SCROLL },
};
#define NKEYS ((int)(sizeof(KEYS) / sizeof(KEYS[0])))

static int is_space(wchar_t c) { return c == L' ' || c == L'\t'; }

static int ieq(const wchar_t *a, const wchar_t *b) { return lstrcmpiW(a, b) == 0; }

/* Parses decimal 1..max from s (whole string). Returns 0 on failure. */
static int parse_small_uint(const wchar_t *s, int max) {
    int v = 0;
    if (!*s || *s == L'0') return 0; /* empty or leading zero ("F012") */
    for (; *s; s++) {
        if (*s < L'0' || *s > L'9') return 0;
        v = v * 10 + (*s - L'0');
        if (v > max) return 0;
    }
    return v;
}

static UINT key_from_token(const wchar_t *t) {
    int i, n;
    wchar_t c = t[0];

    /* Single letter or digit */
    if (c && !t[1]) {
        if (c >= L'a' && c <= L'z') return (UINT)(c - L'a' + L'A');
        if (c >= L'A' && c <= L'Z') return (UINT)c;
        if (c >= L'0' && c <= L'9') return (UINT)c;
        return 0;
    }
    /* F1..F24 */
    if ((c == L'F' || c == L'f') && (n = parse_small_uint(t + 1, 24)) > 0)
        return (UINT)(VK_F1 + n - 1);
    /* Num0..Num9 */
    if ((t[0] == L'N' || t[0] == L'n') && (t[1] == L'U' || t[1] == L'u') &&
        (t[2] == L'M' || t[2] == L'm') && t[3] >= L'0' && t[3] <= L'9' && !t[4])
        return (UINT)(VK_NUMPAD0 + (t[3] - L'0'));

    for (i = 0; i < NKEYS; i++)
        if (ieq(t, KEYS[i].name)) return KEYS[i].vk;
    return 0;
}

static int works_without_modifier(UINT vk) {
    return (vk >= VK_F13 && vk <= VK_F24) || vk == VK_PAUSE || vk == VK_SCROLL;
}

BOOL hotkey_parse(const wchar_t *text, UINT *out_mods, UINT *out_vk) {
    wchar_t tok[MAX_TOKEN + 1];
    const wchar_t *p = text;
    UINT mods = 0, vk = 0;
    int ntok = 0;

    while (is_space(*p)) p++;
    if (ieq(p, L"none")) { *out_mods = 0; *out_vk = 0; return TRUE; }

    for (;;) {
        int len = 0, end;
        UINT bit = 0;

        /* Read one token up to '+' or end. */
        while (is_space(*p)) p++;
        while (*p && *p != L'+') {
            if (len >= MAX_TOKEN) return FALSE; /* too long: reject */
            tok[len++] = *p++;
        }
        while (len > 0 && is_space(tok[len - 1])) len--;
        tok[len] = 0;
        if (len == 0) return FALSE;            /* empty token, e.g. "Ctrl++A" */
        if (++ntok > MAX_TOKENS) return FALSE;

        if (ieq(tok, L"ctrl") || ieq(tok, L"control")) bit = MOD_CONTROL;
        else if (ieq(tok, L"alt"))                     bit = MOD_ALT;
        else if (ieq(tok, L"shift"))                   bit = MOD_SHIFT;
        else if (ieq(tok, L"win") || ieq(tok, L"windows")) bit = MOD_WIN;

        if (bit) {
            if (mods & bit) return FALSE;      /* duplicate modifier */
            mods |= bit;
        } else {
            UINT k = key_from_token(tok);
            if (!k || vk) return FALSE;        /* unknown key, or two keys */
            vk = k;
        }

        end = (*p == 0);
        if (!end) p++;                         /* skip '+' */
        if (end) break;
        if (*p == 0) return FALSE;             /* trailing '+' */
    }

    if (!vk) return FALSE;
    if (!mods && !works_without_modifier(vk)) return FALSE;

    *out_mods = mods;
    *out_vk = vk;
    return TRUE;
}

/* ---- formatting ---- */

static void append(wchar_t *out, int cap, int *pos, const wchar_t *s) {
    while (*s && *pos < cap - 1) out[(*pos)++] = *s++;
    out[*pos] = 0;
}

void hotkey_format(UINT mods, UINT vk, wchar_t *out, int cap) {
    int pos = 0, i;
    wchar_t tmp[8];

    if (cap <= 0) return;
    out[0] = 0;
    if (!vk) { append(out, cap, &pos, L"None"); return; }

    if (mods & MOD_CONTROL) append(out, cap, &pos, L"Ctrl+");
    if (mods & MOD_ALT)     append(out, cap, &pos, L"Alt+");
    if (mods & MOD_SHIFT)   append(out, cap, &pos, L"Shift+");
    if (mods & MOD_WIN)     append(out, cap, &pos, L"Win+");

    if ((vk >= L'A' && vk <= L'Z') || (vk >= L'0' && vk <= L'9')) {
        tmp[0] = (wchar_t)vk; tmp[1] = 0;
        append(out, cap, &pos, tmp);
        return;
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        UINT n = vk - VK_F1 + 1;
        i = 0;
        tmp[i++] = L'F';
        if (n >= 10) tmp[i++] = (wchar_t)(L'0' + n / 10);
        tmp[i++] = (wchar_t)(L'0' + n % 10);
        tmp[i] = 0;
        append(out, cap, &pos, tmp);
        return;
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        tmp[0] = L'N'; tmp[1] = L'u'; tmp[2] = L'm';
        tmp[3] = (wchar_t)(L'0' + (vk - VK_NUMPAD0)); tmp[4] = 0;
        append(out, cap, &pos, tmp);
        return;
    }
    for (i = 0; i < NKEYS; i++) {
        if (KEYS[i].vk == vk) { append(out, cap, &pos, KEYS[i].name); return; }
    }
    append(out, cap, &pos, L"?");
}
