/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdint.h>
#include <string.h>

/* Exact nearest-neighbor 2:1 RGB888 reduction. Pack four output pixels at a
 * time using alias-safe word loads instead of twelve separate byte stores.
 * The packed path is little-endian; the portable tail also covers other CPUs.
 * Input has twice as many pixels as the requested output count. */
static inline void lcd_rgb_half_line(const uint8_t *src, uint8_t *dst, uint32_t pixels)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    while (pixels >= 4) {
        uint32_t p[6], q[3];
        memcpy(p, src, sizeof(p));
        q[0] = (p[0] & 0xffffffu) | ((p[1] & 0xff0000u) << 8);
        q[1] = (p[1] >> 24) | ((p[2] & 0xffu) << 8) | ((p[3] & 0xffffu) << 16);
        q[2] = ((p[3] >> 16) & 0xffu) | ((p[4] >> 16) << 8) | ((p[5] & 0xffu) << 24);
        memcpy(dst, q, sizeof(q));
        src += 24;
        dst += 12;
        pixels -= 4;
    }
#endif
    while (pixels--) {
        memcpy(dst, src, 3);
        src += 6;
        dst += 3;
    }
}
