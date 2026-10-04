/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include "../kvm_display/drivers/font5x7.h"

static inline const char *lcd_status_state(bool signal, bool too_fast, int64_t age_us)
{
    if (!signal) return "NO SIGNAL";
    if (too_fast) return "UNSUPPORTED";
    return age_us < 0 || age_us >= 2000000 ? "STALE" : "LIVE";
}

/* Fixed, opaque 20px strip. Write only to a retired LCD framebuffer, never
 * the CSI source: browser video and screenshots must stay overlay-free. */
static inline void lcd_status_strip(uint8_t *rgb, unsigned w, unsigned h,
                                    const char *text)
{
    if (!rgb || !text || w < 12 || h < 20) return;
    memset(rgb, 0, (size_t)w * 20 * 3);
    unsigned x = 4;
    for (; *text && x + 10 <= w; ++text, x += 12) {
        unsigned ch = (unsigned char)*text;
        const uint8_t *g = font5x7[(ch < 32 || ch > 127) ? 0 : ch - 32];
        for (unsigned col = 0; col < 5; ++col) {
            for (unsigned row = 0; row < 7; ++row) {
                if (!(g[col] & (1u << row))) continue;
                for (unsigned dy = 0; dy < 2; ++dy) {
                    for (unsigned dx = 0; dx < 2; ++dx) {
                        size_t p = ((size_t)(3 + row * 2 + dy) * w + x + col * 2 + dx) * 3;
                        rgb[p] = rgb[p + 1] = rgb[p + 2] = 224;
                    }
                }
            }
        }
    }
}
