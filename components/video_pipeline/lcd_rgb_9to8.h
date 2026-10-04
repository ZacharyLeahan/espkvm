/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
#include <string.h>

/* Exact nearest-neighbor 720->640 RGB888 reduction: each nine source pixels
 * contribute their first eight. Copy runs instead of selecting every pixel.
 * Source contains ceil(pixels * 9 / 8) pixels; unaligned pointers are valid. */
static inline void lcd_rgb_9to8_line(const uint8_t *src, uint8_t *dst, uint32_t pixels)
{
    while (pixels >= 8) {
        memcpy(dst, src, 24);
        src += 27;
        dst += 24;
        pixels -= 8;
    }
    memcpy(dst, src, (size_t)pixels * 3);
}
