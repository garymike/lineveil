#ifndef LINEVEIL_RENDER_H
#define LINEVEIL_RENDER_H

#include <windows.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A renderer owns how pixels reach DWM for one overlay window.
   Window creation, placement, and lifetime stay in main.c. */
typedef struct Renderer {
    const wchar_t *name;
    DWORD ex_style;   /* extra WS_EX_* bits this renderer needs */
    BOOL (*available)(void);
    BOOL (*apply)(HWND hwnd, int width, int height, const Config *cfg);
    void (*release)(HWND hwnd);
} Renderer;

extern const Renderer renderer_gdi;
extern const Renderer renderer_dcomp;

#ifdef __cplusplus
}
#endif

#endif
