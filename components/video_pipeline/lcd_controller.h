/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "lcd_status_strip.h"
#include <math.h>

enum { LCD_UI_NONE, LCD_UI_STATS, LCD_UI_BOTH, LCD_UI_PAD, LCD_UI_BT };
static inline bool lcd_ui_stats(unsigned mode) { return mode == LCD_UI_STATS || mode == LCD_UI_BOTH; }
static inline bool lcd_ui_pad(unsigned mode) { return mode == LCD_UI_BOTH || mode == LCD_UI_PAD; }
static inline unsigned lcd_ui_next(unsigned mode, bool locked)
{
#if CONFIG_KVM_XBOX_BLE
    return locked ? mode : (mode + 1) % 5;
#else
    return locked ? mode : (mode + 1) % 4;
#endif
}
static inline bool lcd_ui_bar(unsigned y, unsigned margin)
{ if (margin < 16) margin = 16; return y < margin || (y < 480 && y >= 480-margin); }

typedef struct { unsigned x,y,w,h; uint8_t key; const char *label; } lcd_pad_button_t;
static const lcd_pad_button_t lcd_pad_buttons[] = {
    {12,240,62,36,0x14,"LT"}, {566,240,62,36,0x08,"RT"},
    {166,240,68,36,0x2a,"BACK"}, {242,240,68,36,0x28,"START"},
    {318,240,68,36,0x05,"BLACK"}, {394,240,68,36,0x11,"WHITE"},
    {58,294,46,46,0x52,"UP"}, {12,340,46,46,0x50,"<"},
    {104,340,46,46,0x4f,">"}, {58,386,46,46,0x51,"DN"},
    {526,286,46,46,0x19,"Y"}, {480,332,46,46,0x06,"X"},
    {572,332,46,46,0x1b,"B"}, {526,378,46,46,0x1d,"A"},
    {200,418,64,36,0x15,"L3"}, {364,418,64,36,0x17,"R3"},
};
static inline void lcd_pad_add(uint8_t keys[6], uint8_t key)
{
    for (unsigned i=0;i<6;i++) { if(keys[i]==key) return; if(!keys[i]) { keys[i]=key; return; } }
}
/* Radial 8px dead zone; continuous magnitude to full tilt at the 52px rim.
 * Screen Y grows down, Xbox stick Y grows up. No keyboard quantization. */
static inline void lcd_pad_axis(int dx,int dy,int16_t out[2])
{
    float radius=sqrtf((float)dx*dx+(float)dy*dy);
    out[0]=out[1]=0;
    if(radius<=8.0f) return;
    float magnitude=fminf((radius-8.0f)/44.0f,1.0f);
    out[0]=(int16_t)lroundf(dx/radius*magnitude*32767.0f);
    out[1]=(int16_t)lroundf(-dy/radius*magnitude*32767.0f);
}
static inline bool lcd_pad_hit(unsigned x,unsigned y,uint8_t keys[6],int16_t axes[4])
{
    for(unsigned i=0;i<sizeof(lcd_pad_buttons)/sizeof(lcd_pad_buttons[0]);i++) {
        const lcd_pad_button_t *b=&lcd_pad_buttons[i];
        if(x>=b->x && x<b->x+b->w && y>=b->y && y<b->y+b->h) { lcd_pad_add(keys,b->key); return true; }
    }
    for(unsigned i=0;i<2;i++) {
        int dx=(int)x-(i?396:232), dy=(int)y-354;
        if(dx*dx+dy*dy > 52*52) continue;
        lcd_pad_axis(dx,dy,&axes[2*i]);
        return true;
    }
    return false;
}
static inline void lcd_pad_rect(uint8_t *fb,unsigned x,unsigned y,unsigned w,unsigned h,uint8_t shade)
{
    for(unsigned yy=y;yy<y+h;yy++) for(unsigned xx=x;xx<x+w;xx++) {
        size_t p=((size_t)yy*640+xx)*3; fb[p]=fb[p+1]=fb[p+2]=shade;
    }
}
static inline void lcd_pad_text(uint8_t *fb,unsigned x,unsigned y,const char *s)
{
    for(unsigned i=0;s[i];i++) for(unsigned c=0;c<5;c++) for(unsigned r=0;r<7;r++)
        if(font5x7[(unsigned char)s[i]-32][c] & (1u<<r)) lcd_pad_rect(fb,x+i*12+c*2,y+r*2,2,2,240);
}
static inline void lcd_pad_draw(uint8_t *fb)
{
    for(unsigned i=0;i<sizeof(lcd_pad_buttons)/sizeof(lcd_pad_buttons[0]);i++) {
        const lcd_pad_button_t *b=&lcd_pad_buttons[i];
        lcd_pad_rect(fb,b->x,b->y,b->w,b->h,150);
        lcd_pad_rect(fb,b->x+2,b->y+2,b->w-4,b->h-4,28);
        lcd_pad_text(fb,b->x+(b->w-strlen(b->label)*12)/2,b->y+(b->h-14)/2,b->label);
    }
    for(unsigned i=0;i<2;i++) {
        int cx=i?396:232;
        for(int y=-52;y<=52;y++) for(int x=-52;x<=52;x++) {
            int d=x*x+y*y;
            if(d<=52*52) lcd_pad_rect(fb,cx+x,354+y,1,1,d>=49*49?150:28);
        }
        lcd_pad_text(fb,cx-6,347,i?"R":"L");
    }
}
