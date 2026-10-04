/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool down;
    int64_t next_press_us;
} lcd_touch_toggle_t;

/* A not-ready packet is NOT a release. Only valid controller reports change
 * the latch; held fingers and multiple fingers form one gesture. */
static inline bool lcd_touch_toggle_event(lcd_touch_toggle_t *s, uint8_t report, int64_t now)
{
    if (!(report & 0x80) || (report & 15) > 5) return false;
    const bool down = (report & 15) != 0;
    const bool toggle = down && !s->down && now >= s->next_press_us;
    s->down = down;
    if (toggle) s->next_press_us = now + 250000;
    return toggle;
}
