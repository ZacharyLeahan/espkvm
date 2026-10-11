/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "../components/video_pipeline/lcd_rgb_half.h"

int main(void)
{
    uint8_t src[640 * 6 + 8], dst[640 * 3 + 8];
    uint32_t random = 1;
    for (unsigned trial = 0; trial < 16; ++trial) {
        for (unsigned i = 0; i < sizeof(src); ++i) {
            random = random * 1664525u + 1013904223u;
            src[i] = random >> 24;
        }
        for (unsigned offset = 0; offset < 4; ++offset) {
            for (unsigned n = 1; n <= 640; ++n) {
                memset(dst, 0xa5, sizeof(dst));
                lcd_rgb_half_line(src + offset, dst + offset, n);
                for (unsigned x = 0; x < n; ++x) {
                    assert(memcmp(dst + offset + x * 3, src + offset + x * 6, 3) == 0);
                }
                assert(dst[offset + n * 3] == 0xa5);
                if (offset) assert(dst[offset - 1] == 0xa5);
            }
        }
    }
    puts("RGB half-scale matches reference, including tails and unaligned buffers");
    return 0;
}
