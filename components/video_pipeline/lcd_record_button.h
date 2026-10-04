/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "lcd_status_strip.h"
#include <stdio.h>

#define LCD_REC_W 160u
#define LCD_REC_H 44u

static inline void lcd_record_pixel(uint8_t *tile, unsigned x, unsigned y,
                                     uint8_t r, uint8_t g, uint8_t b)
{
    if (x >= LCD_REC_W || y >= LCD_REC_H) return;
    size_t p = ((size_t)y * LCD_REC_W + x) * 3;
    /* This DPI framebuffer displays packed RGB888 in B,G,R byte order.
     * Convert authored UI colors here; leave captured game pixels untouched. */
    tile[p] = b; tile[p + 1] = g; tile[p + 2] = r;
}

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
        /* Raised 40px action button stays at the same top-right position.
         * Idle: red record circle. Active: white stop square, separate live
         * red dot and timer. Busy: amber ellipsis, never a false recording. */
        for (unsigned y = 2; y < 42; ++y) {
            for (unsigned x = 116; x < 158; ++x) {
                bool edge = x == 116 || x == 157 || y == 2 || y == 41;
                uint8_t shade = edge ? 150 : 42;
                lcd_record_pixel(tile, x, y, shade, shade, shade);
            }
        }
        for (int y = 8; y <= 35; ++y) {
            for (int x = 122; x <= 151; ++x) {
                int dx = x - 137, dy = y - 22;
                if (busy) {
                    if (y >= 20 && y <= 24 &&
                        ((x >= 125 && x <= 129) || (x >= 135 && x <= 139) ||
                         (x >= 145 && x <= 149)))
                        lcd_record_pixel(tile, x, y, 255, 190, 45);
                } else if (active) {
                    if (x >= 128 && x <= 146 && y >= 13 && y <= 31)
                        lcd_record_pixel(tile, x, y, 245, 245, 245);
                } else if (dx * dx + dy * dy <= 121) {
                    lcd_record_pixel(tile, x, y, 245, 45, 55);
                }
            }
        }
        if (active) {
            unsigned long minutes = seconds / 60;
            if (minutes > 9999) minutes = 9999;
            snprintf(label, sizeof(label), "%02lu:%02lu", minutes,
                     (unsigned long)(seconds % 60));
            for (unsigned i = 0; label[i] && i < 7; ++i) {
                const uint8_t *glyph = font5x7[(unsigned char)label[i] - 32];
                for (unsigned col = 0; col < 5; ++col)
                    for (unsigned row = 0; row < 7; ++row)
                        if (glyph[col] & (1u << row))
                            for (unsigned dy = 0; dy < 2; ++dy)
                                for (unsigned dx = 0; dx < 2; ++dx)
                                    lcd_record_pixel(tile, 22 + i * 12 + col * 2 + dx,
                                                     15 + row * 2 + dy, 240, 240, 240);
            }
            for (int y = 17; y <= 27; ++y)
                for (int x = 5; x <= 15; ++x)
                    if ((x - 10) * (x - 10) + (y - 22) * (y - 22) <= 25)
                        lcd_record_pixel(tile, x, y, seconds & 1u ? 180 : 255, 30, 40);
        }
        ready = true; last_active = active; last_busy = busy; last_seconds = seconds;
    }
    for (unsigned y = 0; y < LCD_REC_H; ++y)
        memcpy(rgb + ((size_t)y * w + w - LCD_REC_W) * 3,
               tile + (size_t)y * LCD_REC_W * 3, LCD_REC_W * 3);
}
