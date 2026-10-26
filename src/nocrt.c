/* Minimal stand-ins for the CRT. The compiler may emit calls to memset/memcpy
 * for struct initialization or copies, even with no CRT linked.
 * volatile stops the optimizer from turning these loops back into calls to
 * themselves.
 */
#include <stddef.h>

#ifdef _MSC_VER
#pragma function(memset)
#pragma function(memcpy)
/* MSVC emits a reference to this whenever floating point is used
   (the DComp renderer's scale transform). Normally the CRT defines it. */
int _fltused = 0;
#endif

void *memset(void *dst, int c, size_t n) {
    volatile unsigned char *p = (volatile unsigned char *)dst;
    while (n--) *p++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    volatile unsigned char *d = (volatile unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}
