/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "lcd_status_strip.h"
#include <stdio.h>

#define LCD_REC_W 160u
#define LCD_REC_H 44u

static inline bool lcd_record_hit(unsigned x, unsigned y, unsigned w, unsigned h)
{
    return w >= LCD_REC_W && h >= LCD_REC_H && x < w && y < LCD_REC_H &&
           x >= w - LCD_REC_W;
}

static inline void lcd_record_button(uint8_t *rgb, unsigned w, unsigned h,
                                      bool active, bool busy, uint32_t seconds)
{
    if (!rgb || w < LCD_REC_W || h < LCD_REC_H) return;
    /* Only the capture task draws overlays. Rasterize on state/timer changes,
     * not thirty times a second while the CPU also scales and encodes video. */
    static uint8_t tile[LCD_REC_W * LCD_REC_H * 3];
    static bool ready, last_active, last_busy;
    static uint32_t last_seconds;
    if (!ready || active != last_active || busy != last_busy || seconds != last_seconds) {
        char label[16];
        memset(tile, 0, sizeof(tile));
        lcd_status_strip(tile, LCD_REC_W, 20, busy ? "WAIT..." : active ? "STOP" : "REC");
        snprintf(label, sizeof(label), "%02lu:%02lu", (unsigned long)(seconds / 60),
                 (unsigned long)(seconds % 60));
        lcd_status_strip(tile + 22 * LCD_REC_W * 3, LCD_REC_W, 20, active ? label : "SD VIDEO");
        for (size_t p = 0; p < sizeof(tile); p += 3) {
            if (!tile[p]) { tile[p] = active ? 120 : 32; tile[p + 1] = tile[p + 2] = 16; }
        }
        ready = true; last_active = active; last_busy = busy; last_seconds = seconds;
    }
    for (unsigned y = 0; y < LCD_REC_H; ++y)
        memcpy(rgb + ((size_t)y * w + w - LCD_REC_W) * 3,
               tile + (size_t)y * LCD_REC_W * 3, LCD_REC_W * 3);
}
