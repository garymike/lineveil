/* Native unit tests for the hotkey parser: make test */
#include <stdio.h>
#include <string.h>
#include "../src/config.h"

static int failures, total;

static void expect_ok(const wchar_t *in, UINT mods, UINT vk, const wchar_t *formatted) {
    UINT m = 0xDEAD, v = 0xBEEF;
    wchar_t out[40];
    total++;
    if (!hotkey_parse(in, &m, &v)) {
        printf("FAIL  parse rejected: \"%ls\"\n", in); failures++; return;
    }
    if (m != mods || v != vk) {
        printf("FAIL  \"%ls\": got mods=%u vk=0x%X, want mods=%u vk=0x%X\n", in, m, v, mods, vk);
        failures++; return;
    }
    hotkey_format(m, v, out, 40);
    if (wcscmp(out, formatted) != 0) {
        printf("FAIL  \"%ls\": formatted \"%ls\", want \"%ls\"\n", in, out, formatted);
        failures++;
    }
}

static void expect_reject(const wchar_t *in) {
    UINT m = 0xDEAD, v = 0xBEEF;
    total++;
    if (hotkey_parse(in, &m, &v)) {
        printf("FAIL  should reject: \"%ls\" (got mods=%u vk=0x%X)\n", in, m, v); failures++;
    } else if (m != 0xDEAD || v != 0xBEEF) {
        printf("FAIL  rejected but wrote outputs: \"%ls\"\n", in); failures++;
    }
}

int main(void) {
    /* Accepted */
    expect_ok(L"Ctrl+Alt+F12",        MOD_CONTROL | MOD_ALT, VK_F12, L"Ctrl+Alt+F12");
    expect_ok(L"ctrl+alt+f12",        MOD_CONTROL | MOD_ALT, VK_F12, L"Ctrl+Alt+F12");
    expect_ok(L"  Alt + Ctrl + F12 ", MOD_CONTROL | MOD_ALT, VK_F12, L"Ctrl+Alt+F12");
    expect_ok(L"Shift+Win+s",         MOD_SHIFT | MOD_WIN, 'S', L"Shift+Win+S");
    expect_ok(L"Control+Windows+9",   MOD_CONTROL | MOD_WIN, '9', L"Ctrl+Win+9");
    expect_ok(L"Ctrl+F1",             MOD_CONTROL, VK_F1, L"Ctrl+F1");
    expect_ok(L"Alt+F24",             MOD_ALT, VK_F24, L"Alt+F24");
    expect_ok(L"F13",                 0, VK_F13, L"F13");
    expect_ok(L"Pause",               0, VK_PAUSE, L"Pause");
    expect_ok(L"scrolllock",          0, VK_SCROLL, L"ScrollLock");
    expect_ok(L"Ctrl+Num5",           MOD_CONTROL, VK_NUMPAD0 + 5, L"Ctrl+Num5");
    expect_ok(L"Ctrl+Shift+PgDn",     MOD_CONTROL | MOD_SHIFT, VK_NEXT, L"Ctrl+Shift+PageDown");
    expect_ok(L"Alt+Escape",          MOD_ALT, VK_ESCAPE, L"Alt+Esc");
    expect_ok(L"Ctrl+Alt+Shift+Win+Space", MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN, VK_SPACE,
              L"Ctrl+Alt+Shift+Win+Space");
    expect_ok(L"None",                0, 0, L"None");
    expect_ok(L"none",                0, 0, L"None");

    /* Rejected */
    expect_reject(L"");
    expect_reject(L"   ");
    expect_reject(L"Ctrl");               /* no key */
    expect_reject(L"Ctrl+Alt");           /* no key */
    expect_reject(L"A");                  /* bare letter would hijack typing */
    expect_reject(L"F12");                /* bare common F-key */
    expect_reject(L"Space");
    expect_reject(L"Ctrl+Ctrl+A");        /* duplicate modifier */
    expect_reject(L"Ctrl+A+B");           /* two keys */
    expect_reject(L"Ctrl++A");            /* empty token */
    expect_reject(L"+Ctrl+A");
    expect_reject(L"Ctrl+A+");            /* trailing plus */
    expect_reject(L"Ctrl+A+ ");
    expect_reject(L"Ctrl+F0");
    expect_reject(L"Ctrl+F25");
    expect_reject(L"Ctrl+F012");
    expect_reject(L"Ctrl+F99999999999");  /* overflow attempt */
    expect_reject(L"Ctrl+Num10");
    expect_reject(L"Ctrl+Hyper");         /* unknown */
    expect_reject(L"Ctrl+é");
    expect_reject(L"Ctrl+AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"); /* over token cap */
    expect_reject(L"Ctrl+Alt+Shift+Win+Ctrl+Alt+A");            /* too many tokens */
    expect_reject(L"Ctrl+Alt+F12 junk");

    printf("%d/%d passed\n", total - failures, total);
    return failures ? 1 : 0;
}
