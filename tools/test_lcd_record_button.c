/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdlib.h>
#include "../components/video_pipeline/lcd_record_button.h"

int main(void)
{
    assert(lcd_record_hit(480, 0, 640, 480));
    assert(lcd_record_hit(639, 43, 640, 480));
    assert(!lcd_record_hit(479, 0, 640, 480));
    assert(!lcd_record_hit(640, 0, 640, 480));
    assert(!lcd_record_hit(500, 44, 640, 480));
    assert(!lcd_record_hit(0, 0, 80, 40));
    size_t size = 640 * 480 * 3;
    uint8_t *buf = malloc(size);
    assert(buf);
    memset(buf, 0x5a, size);
    lcd_record_button(buf, 640, 480, false, false, 0);
    size_t icon = (22 * 640 + 480 + 137) * 3;
    assert(buf[icon] == 55 && buf[icon + 1] == 45 && buf[icon + 2] == 245);
    size_t dot = (22 * 640 + 480 + 10) * 3;
    assert(buf[dot] == 0);
    lcd_record_button(buf, 640, 480, true, false, 3661);
    assert(buf[icon] == 245 && buf[icon + 1] == 245);
    assert(buf[dot] == 40 && buf[dot + 1] == 30 && buf[dot + 2] == 180);
    lcd_record_button(buf, 640, 480, true, true, UINT32_MAX);
    assert(buf[icon] == 45 && buf[icon + 1] == 190 && buf[icon + 2] == 255);
    for (unsigned y = 0; y < 480; ++y)
        for (unsigned x = 0; x < 640; ++x)
            if (!lcd_record_hit(x, y, 640, 480))
                for (unsigned c = 0; c < 3; ++c)
                    assert(buf[(y * 640 + x) * 3 + c] == 0x5a);
    free(buf);
    return 0;
}
