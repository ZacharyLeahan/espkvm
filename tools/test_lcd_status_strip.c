/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../components/video_pipeline/lcd_status_strip.h"

int main(void)
{
    assert(!strcmp(lcd_status_state(false, false, 0), "NO SIGNAL"));
    assert(!strcmp(lcd_status_state(true, true, 0), "UNSUPPORTED"));
    assert(!strcmp(lcd_status_state(true, false, -1), "STALE"));
    assert(!strcmp(lcd_status_state(true, false, 1999999), "LIVE"));
    assert(!strcmp(lcd_status_state(true, false, 2000000), "STALE"));
    for (unsigned w = 1; w <= 640; ++w) {
        const size_t n = (size_t)w * 24 * 3;
        uint8_t *p = malloc(n + 2);
        assert(p);
        memset(p, 91, n + 2);
        lcd_status_strip(p + 1, w, 24,
                         "LIVE 720p60Hz LCD 28.5 V:1 REMOTE ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        assert(p[0] == 91 && p[n + 1] == 91);
        for (size_t i = (size_t)w * 20 * 3; i < n; ++i) assert(p[i + 1] == 91);
        free(p);
    }
    uint8_t tiny[12 * 19 * 3];
    memset(tiny, 91, sizeof(tiny));
    lcd_status_strip(tiny, 12, 19, "A");
    for (size_t i = 0; i < sizeof(tiny); ++i) assert(tiny[i] == 91);
    puts("LCD strip clipping, untouched image rows, and freshness states passed");
}
