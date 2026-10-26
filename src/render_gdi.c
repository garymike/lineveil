/* GDI renderer: one-shot UpdateLayeredWindow.
 *
 * Builds a premultiplied 32-bit pattern once, hands it to DWM, then frees it.
 * After apply() returns, this process holds no pixel memory; DWM keeps its own
 * copy of the surface (width * height * 4 bytes per monitor). No redraws ever
 * happen unless config or display layout changes.
 */
#include "render.h"

static BOOL gdi_available(void) { return TRUE; }

static BOOL gdi_apply(HWND hwnd, int w, int h, const Config *c) {
    BITMAPINFO bi;
    void *bits = NULL;
    HDC screen, mem;
    HBITMAP bmp;
    HGDIOBJ old;
    POINT src = {0, 0};
    SIZE size;
    BLENDFUNCTION bf;
    DWORD a, r, g, b, px;
    DWORD *base;
    int x, y;
    BOOL ok;

    if (w <= 0 || h <= 0) return FALSE;

    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; /* top-down */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    screen = GetDC(NULL);
    if (!screen) return FALSE;
    mem = CreateCompatibleDC(screen);
    if (!mem) { ReleaseDC(NULL, screen); return FALSE; }

    /* DIB sections are zero-filled: fully transparent to start. */
    bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(mem);
        ReleaseDC(NULL, screen);
        return FALSE;
    }
    old = SelectObject(mem, bmp);

    /* Premultiplied BGRA, as required by ULW_ALPHA + AC_SRC_ALPHA. */
    a = (DWORD)c->opacity;
    r = ((c->color >> 16) & 0xFF) * a / 255;
    g = ((c->color >> 8) & 0xFF) * a / 255;
    b = (c->color & 0xFF) * a / 255;
    px = (a << 24) | (r << 16) | (g << 8) | b;

    base = (DWORD *)bits;
    if (!c->vertical) {
        for (y = 0; y < h; y++) {
            if (y % c->period < c->thickness) {
                DWORD *row = base + (SIZE_T)y * (SIZE_T)w;
                for (x = 0; x < w; x++) row[x] = px;
            }
        }
    } else {
        for (y = 0; y < h; y++) {
            DWORD *row = base + (SIZE_T)y * (SIZE_T)w;
            for (x = 0; x < w; x++)
                if (x % c->period < c->thickness) row[x] = px;
        }
    }

    size.cx = w;
    size.cy = h;
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;

    ok = UpdateLayeredWindow(hwnd, screen, NULL, &size, mem, &src, 0, &bf, ULW_ALPHA);

    /* DWM has its copy now; drop ours. */
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
    return ok;
}

static void gdi_release(HWND hwnd) { (void)hwnd; }

const Renderer renderer_gdi = {
    L"gdi",
    WS_EX_LAYERED,
    gdi_available,
    gdi_apply,
    gdi_release,
};
