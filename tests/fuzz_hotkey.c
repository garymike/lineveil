/* libFuzzer harness for the hotkey parser.
 *   make fuzz   (needs clang)
 * Properties checked:
 *   1. no crashes / memory errors on any input (ASan + UBSan)
 *   2. anything accepted round-trips: format(parse(x)) parses back to the same value
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../src/config.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    wchar_t in[96], out[40];
    size_t i, n = size / 2;
    UINT m1, v1, m2, v2;

    if (n > 95) n = 95;
    for (i = 0; i < n; i++)
        in[i] = (wchar_t)(data[2 * i] | (data[2 * i + 1] << 8)); /* UTF-16 units */
    in[n] = 0;

    if (!hotkey_parse(in, &m1, &v1)) return 0;

    hotkey_format(m1, v1, out, 40);
    if (!hotkey_parse(out, &m2, &v2) || m1 != m2 || v1 != v2) abort();
    return 0;
}
