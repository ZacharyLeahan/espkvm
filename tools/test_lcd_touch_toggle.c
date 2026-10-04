/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "../components/video_pipeline/lcd_touch_toggle.h"
int main(void)
{
    lcd_touch_toggle_t s = {0};
    assert(lcd_touch_toggle_event(&s, 0x81, 1000000));
    assert(!lcd_touch_toggle_event(&s, 0, 1100000));
    assert(!lcd_touch_toggle_event(&s, 0x82, 1400000));
    assert(!lcd_touch_toggle_event(&s, 0x81, 4000000));
    assert(!lcd_touch_toggle_event(&s, 0x80, 4100000));
    assert(lcd_touch_toggle_event(&s, 0x81, 4200000));
    assert(!lcd_touch_toggle_event(&s, 0x80, 4210000));
    assert(!lcd_touch_toggle_event(&s, 0x81, 4220000));
    assert(!lcd_touch_toggle_event(&s, 0x81, 5000000));
    assert(!lcd_touch_toggle_event(&s, 0x86, 5100000));
    assert(!lcd_touch_toggle_event(&s, 0x80, 5200000));
    assert(lcd_touch_toggle_event(&s, 0x81, 5300000));
    puts("Touch press/hold/release, invalid packets, multitouch and debounce passed");
}
