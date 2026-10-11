/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "../components/video_pipeline/lcd_controller.h"
int main(void)
{
    assert(lcd_ui_bt_button(452,76) && lcd_ui_bt_button(607,107));
    assert(!lcd_ui_bt_button(451,76) && !lcd_ui_bt_button(608,107));
    assert(!lcd_ui_bt_button(500,75) && !lcd_ui_bt_button(500,108));
    assert(lcd_ui_bt_timing(320,76) && lcd_ui_bt_timing(443,107));
    assert(!lcd_ui_bt_timing(444,90) && !lcd_ui_bt_timing(320,108));
#if CONFIG_KVM_XBOX_BLE
    const unsigned modes=6;
#else
    const unsigned modes=4;
#endif
    for(unsigned i=0;i<modes;i++) {
        assert(lcd_ui_next(i,false)==(i+1)%modes);
        assert(lcd_ui_next(i,true)==i);
    }
#if CONFIG_KVM_XBOX_BLE
    assert(lcd_ui_next(LCD_UI_BT,false)==LCD_UI_INPUTS);
    assert(lcd_ui_next(LCD_UI_INPUTS,false)==LCD_UI_NONE);
#endif
    assert(!lcd_ui_stats(LCD_UI_INPUTS) && !lcd_ui_pad(LCD_UI_INPUTS));
    assert(lcd_ui_cycle_hit(LCD_UI_INPUTS,448,0));
    assert(!lcd_ui_cycle_hit(LCD_UI_NONE,448,0));
    assert(!lcd_ui_cycle_hit(LCD_UI_INPUTS,480,0));
    char rows[3][48]={"A > LEFT+X","",""},line[51];
    lcd_input_recent(rows,line); assert(!strcmp(line,"A > LEFT+X"));
    strcpy(rows[0],"START > LEFT+X > A > RIGHT+B");
    strcpy(rows[1]," > LB+RB > UP+Y > LEFT+X");
    strcpy(rows[2]," > DOWN+B");
    lcd_input_recent(rows,line);
    assert(strlen(line)<=50 && strstr(line,"DOWN+B") && strncmp(line," > ",3));
    memset(rows,0,sizeof(rows));lcd_input_recent(rows,line); assert(!line[0]);
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
