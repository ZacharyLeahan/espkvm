/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "../components/video_pipeline/lcd_controller.h"
int main(void)
{
    for(unsigned i=0;i<4;i++) {
        assert(lcd_ui_next(i,false)==(i+1)%4);
        assert(lcd_ui_next(i,true)==i);
    }
    assert(!lcd_ui_stats(0) && lcd_ui_stats(1) && lcd_ui_stats(2) && !lcd_ui_stats(3));
    assert(!lcd_ui_pad(0) && !lcd_ui_pad(1) && lcd_ui_pad(2) && lcd_ui_pad(3));
    assert(lcd_ui_bar(0,60) && lcd_ui_bar(479,60) && !lcd_ui_bar(100,60));
    assert(lcd_ui_bar(15,0) && !lcd_ui_bar(16,0));
    for(unsigned i=0;i<sizeof(lcd_pad_buttons)/sizeof(lcd_pad_buttons[0]);i++) {
        uint8_t keys[6]={0}; const lcd_pad_button_t *b=&lcd_pad_buttons[i];
        assert(lcd_pad_hit(b->x+b->w/2,b->y+b->h/2,keys));
        assert(keys[0]==b->key);
    }
    uint8_t keys[6]={0};
    assert(lcd_pad_hit(232,354,keys) && !keys[0]);
    assert(lcd_pad_hit(265,330,keys) && keys[0]==7 && keys[1]==26);
    assert(lcd_pad_hit(366,384,keys) && keys[2]==13 && keys[3]==14);
    assert(!lcd_pad_hit(320,470,keys));
    static uint8_t fb[640*480*3];
    lcd_pad_draw(fb);
    puts("LCD mode cycle, recording lock, hitboxes, stick diagonals and rendering: PASS");
}
