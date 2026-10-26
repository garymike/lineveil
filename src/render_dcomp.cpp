/* DirectComposition renderer (renderer=dcomp, Windows 8+).
 *
 * Instead of a full-screen bitmap, each overlay gets a 1-pixel-wide strip
 * (horizontal mode: 1 x height; vertical: width x 1). DWM stretches it across
 * the monitor with nearest-neighbor sampling, so lines stay crisp while the
 * surface costs a few KB instead of ~33 MB per 4K monitor.
 *
 * d3d11.dll and dcomp.dll are loaded at runtime, only when this renderer is
 * selected. GDI mode never maps the GPU stack into the process, and the exe
 * still launches on Windows 7 (where dcomp.dll doesn't exist).
 *
 * Tradeoff to measure: a D3D device loads GPU driver DLLs into our process,
 * which may cost more private working set than the DWM memory it saves.
 *
 * Plain C++ subset: no exceptions, no RTTI, no new/delete, no CRT.
 */
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dcomp.h>
#include "render.h"

typedef HRESULT (WINAPI *PFN_CreateD3D11Device)(
    IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *,
    UINT, UINT, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
typedef HRESULT (WINAPI *PFN_CreateDCompDevice)(IDXGIDevice *, REFIID, void **);

namespace {

const int kMaxSlots = 16; /* matches MAX_MONITORS in main.c */

struct Slot {
    HWND hwnd;
    IDCompositionTarget *target;
    IDCompositionVisual *visual;
    IDCompositionSurface *surface;
};

HMODULE g_d3d11_dll;
HMODULE g_dcomp_dll;
PFN_CreateD3D11Device g_create_d3d;
PFN_CreateDCompDevice g_create_dcomp;
BOOL g_load_tried;

ID3D11Device *g_dev;
ID3D11DeviceContext *g_ctx;
IDCompositionDevice *g_dc;

Slot g_slots[kMaxSlots];
int g_live;

template <class T> void safe_release(T *&p) {
    if (p) { p->Release(); p = 0; }
}

BOOL load_libs() {
    if (g_load_tried) return g_create_d3d && g_create_dcomp;
    g_load_tried = TRUE;

    /* System32 only, so a planted DLL next to the exe can't be picked up.
       dcomp.dll exists on 8+, where this flag is always supported. */
    g_dcomp_dll = LoadLibraryExW(L"dcomp.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!g_dcomp_dll) return FALSE;
    g_d3d11_dll = LoadLibraryExW(L"d3d11.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!g_d3d11_dll) return FALSE;

    g_create_dcomp = (PFN_CreateDCompDevice)(void *)GetProcAddress(g_dcomp_dll, "DCompositionCreateDevice");
    g_create_d3d = (PFN_CreateD3D11Device)(void *)GetProcAddress(g_d3d11_dll, "D3D11CreateDevice");
    return g_create_d3d && g_create_dcomp;
}

void release_device() {
    safe_release(g_dc);
    safe_release(g_ctx);
    safe_release(g_dev);
}

BOOL ensure_device() {
    IDXGIDevice *dxgi = 0;
    HRESULT hr;

    if (g_dc) return TRUE;
    if (!load_libs()) return FALSE;

    hr = g_create_d3d(0, D3D_DRIVER_TYPE_HARDWARE, 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                      0, 0, D3D11_SDK_VERSION, &g_dev, 0, &g_ctx);
    if (FAILED(hr)) /* no usable GPU (some VMs): software rasterizer */
        hr = g_create_d3d(0, D3D_DRIVER_TYPE_WARP, 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                          0, 0, D3D11_SDK_VERSION, &g_dev, 0, &g_ctx);
    if (FAILED(hr)) { release_device(); return FALSE; }

    hr = g_dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgi);
    if (SUCCEEDED(hr))
        hr = g_create_dcomp(dxgi, __uuidof(IDCompositionDevice), (void **)&g_dc);
    safe_release(dxgi);
    if (FAILED(hr)) { release_device(); return FALSE; }
    return TRUE;
}

Slot *find_slot(HWND hwnd) {
    for (int i = 0; i < kMaxSlots; i++)
        if (g_slots[i].hwnd == hwnd) return &g_slots[i];
    return 0;
}

void clear_slot(Slot *s) {
    safe_release(s->target);
    safe_release(s->visual);
    safe_release(s->surface);
    s->hwnd = 0;
}

/* Fills the strip: n pixels along the axis the lines repeat on. */
BOOL fill_surface(IDCompositionSurface *surface, int sw, int sh, const Config *c) {
    DWORD a = (DWORD)c->opacity;
    DWORD r = ((c->color >> 16) & 0xFF) * a / 255;
    DWORD g = ((c->color >> 8) & 0xFF) * a / 255;
    DWORD b = (c->color & 0xFF) * a / 255;
    DWORD px = (a << 24) | (r << 16) | (g << 8) | b; /* premultiplied BGRA */
    int n = sw * sh;                                  /* one of sw/sh is 1 */
    ID3D11Texture2D *tex = 0;
    POINT off;
    D3D11_BOX box;
    DWORD *buf;
    HRESULT hr;

    buf = (DWORD *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)n * sizeof(DWORD));
    if (!buf) return FALSE;
    for (int i = 0; i < n; i++)
        buf[i] = pattern_on(c, n, i) ? px : 0;

    hr = surface->BeginDraw(0, __uuidof(ID3D11Texture2D), (void **)&tex, &off);
    if (SUCCEEDED(hr)) {
        /* The surface may live inside a shared atlas, hence the offset. */
        box.left = (UINT)off.x;  box.right = (UINT)(off.x + sw);
        box.top = (UINT)off.y;   box.bottom = (UINT)(off.y + sh);
        box.front = 0;           box.back = 1;
        g_ctx->UpdateSubresource(tex, 0, &box, buf, (UINT)(sw * sizeof(DWORD)), 0);
        safe_release(tex);
        hr = surface->EndDraw();
    }
    HeapFree(GetProcessHeap(), 0, buf);
    return SUCCEEDED(hr);
}

BOOL dcomp_available(void) {
    return load_libs();
}

BOOL dcomp_apply(HWND hwnd, int w, int h, const Config *c) {
    Slot *s;
    int sw, sh;
    D2D_MATRIX_3X2_F m;

    if (w <= 0 || h <= 0 || !ensure_device()) return FALSE;
    if (find_slot(hwnd) || !(s = find_slot(0))) return FALSE;

    /* Layered + fully opaque window: keeps WS_EX_TRANSPARENT click-through
       working while DComp supplies all of the pixels. */
    if (!SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA)) return FALSE;

    sw = c->vertical ? w : 1;
    sh = c->vertical ? 1 : h;

    s->hwnd = hwnd;
    if (FAILED(g_dc->CreateSurface((UINT)sw, (UINT)sh, DXGI_FORMAT_B8G8R8A8_UNORM,
                                   DXGI_ALPHA_MODE_PREMULTIPLIED, &s->surface)) ||
        !fill_surface(s->surface, sw, sh, c) ||
        FAILED(g_dc->CreateVisual(&s->visual))) {
        clear_slot(s);
        return FALSE;
    }

    /* Stretch the strip across the monitor. */
    m._11 = c->vertical ? 1.0f : (float)w;  m._12 = 0.0f;
    m._21 = 0.0f;  m._22 = c->vertical ? (float)h : 1.0f;
    m._31 = 0.0f;  m._32 = 0.0f;

    if (FAILED(s->visual->SetContent(s->surface)) ||
        FAILED(s->visual->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR)) ||
        FAILED(s->visual->SetBorderMode(DCOMPOSITION_BORDER_MODE_HARD)) ||
        FAILED(s->visual->SetTransform(m)) ||
        FAILED(g_dc->CreateTargetForHwnd(hwnd, TRUE, &s->target)) ||
        FAILED(s->target->SetRoot(s->visual)) ||
        FAILED(g_dc->Commit())) {
        clear_slot(s);
        return FALSE;
    }

    g_live++;
    return TRUE;
}

void dcomp_release(HWND hwnd) {
    Slot *s = find_slot(hwnd);
    if (!s || !hwnd) return;
    clear_slot(s);
    if (--g_live == 0) release_device(); /* free GPU memory when nothing is shown */
}

} /* namespace */

extern "C" const Renderer renderer_dcomp = {
    L"dcomp",
    WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP,
    dcomp_available,
    dcomp_apply,
    dcomp_release,
};
