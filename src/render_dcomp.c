/* DirectComposition renderer (STUB).
 *
 * Plan:
 *   - Create a D3D11 device (BGRA support) -> DXGI device -> IDCompositionDevice.
 *   - Target the overlay HWND (created with WS_EX_NOREDIRECTIONBITMAP so no GDI
 *     redirection surface is allocated).
 *   - Create a 1 x height surface (horizontal mode) or width x 1 (vertical),
 *     fill it with the premultiplied pattern: a few KB instead of ~33 MB at 4K.
 *   - Visual with a scale transform to stretch it across the monitor and
 *     DCOMPOSITION_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR so lines stay crisp.
 *   - Commit once. DWM composites it from then on.
 *
 * Note: dcomp.h is C++ only, so the real implementation will be
 * render_dcomp.cpp (no exceptions, no RTTI, still no CRT).
 *
 * Tradeoff to measure: this loads GPU driver DLLs into our process, which may
 * cost more private working set than the GDI path saves in DWM.
 */
#include "render.h"

static BOOL dcomp_available(void) { return FALSE; }

static BOOL dcomp_apply(HWND hwnd, int w, int h, const Config *c) {
    (void)hwnd; (void)w; (void)h; (void)c;
    return FALSE;
}

static void dcomp_release(HWND hwnd) { (void)hwnd; }

const Renderer renderer_dcomp = {
    L"dcomp",
    WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP,
    dcomp_available,
    dcomp_apply,
    dcomp_release,
};
