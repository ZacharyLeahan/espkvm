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
        uint8_t keys[6]={0}; int16_t axes[4]={0}; const lcd_pad_button_t *b=&lcd_pad_buttons[i];
        assert(lcd_pad_hit(b->x+b->w/2,b->y+b->h/2,keys,axes));
        assert(keys[0]==b->key);
    }
    uint8_t keys[6]={0};
    int16_t axes[4]={0};
    assert(lcd_pad_hit(232,354,keys,axes) && !keys[0] && !axes[0] && !axes[1]);
    assert(lcd_pad_hit(265,330,keys,axes) && axes[0]>0 && axes[1]>0 && axes[0]<32767);
    assert(lcd_pad_hit(366,384,keys,axes) && axes[2]<0 && axes[3]<0);
    assert(!lcd_pad_hit(320,470,keys,axes));
    int16_t a[2];
    lcd_pad_axis(8,0,a); assert(a[0]==0 && a[1]==0);
    lcd_pad_axis(30,0,a); assert(a[0]>=16383 && a[0]<=16384 && !a[1]);
    lcd_pad_axis(52,0,a); assert(a[0]==32767 && !a[1]);
    lcd_pad_axis(-52,0,a); assert(a[0]==-32767);
    lcd_pad_axis(0,-52,a); assert(a[1]==32767);
    lcd_pad_axis(0,52,a); assert(a[1]==-32767);
    int previous=0;
    for(int x=0;x<=52;x++) { lcd_pad_axis(x,0,a); assert(a[0]>=previous); previous=a[0]; }
    for(int x=-52;x<=52;x++) for(int y=-52;y<=52;y++) {
        lcd_pad_axis(x,y,a);
        assert((double)a[0]*a[0]+(double)a[1]*a[1] <= 32768.0*32768.0);
    }
    static uint8_t fb[640*480*3];
    lcd_pad_draw(fb);
    puts("LCD mode cycle, recording lock, hitboxes, stick diagonals and rendering: PASS");
}
