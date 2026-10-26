/* Minimal stand-in for <windows.h> so pure-logic files (hotkey.c) can be
   unit-tested natively on Linux/macOS. Only what those files need. */
#ifndef LINEVEIL_TEST_SHIM_WINDOWS_H
#define LINEVEIL_TEST_SHIM_WINDOWS_H

#include <wchar.h>

typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned int DWORD;
#define TRUE 1
#define FALSE 0

#define MOD_ALT     0x0001
#define MOD_CONTROL 0x0002
#define MOD_SHIFT   0x0004
#define MOD_WIN     0x0008

#define VK_BACK     0x08
#define VK_TAB      0x09
#define VK_RETURN   0x0D
#define VK_PAUSE    0x13
#define VK_ESCAPE   0x1B
#define VK_SPACE    0x20
#define VK_PRIOR    0x21
#define VK_NEXT     0x22
#define VK_END      0x23
#define VK_HOME     0x24
#define VK_LEFT     0x25
#define VK_UP       0x26
#define VK_RIGHT    0x27
#define VK_DOWN     0x28
#define VK_INSERT   0x2D
#define VK_DELETE   0x2E
#define VK_NUMPAD0  0x60
#define VK_NUMPAD9  0x69
#define VK_F1       0x70
#define VK_F12      0x7B
#define VK_F13      0x7C
#define VK_F24      0x87
#define VK_SCROLL   0x91

static inline int lstrcmpiW(const wchar_t *a, const wchar_t *b) { return wcscasecmp(a, b); }

/* config.h declares these but tests don't call them. */
typedef struct { DWORD lo, hi; } FILETIME;

#endif
