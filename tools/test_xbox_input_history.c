/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include "../components/kvm_hid/include/xbox_input_history.h"
int main(void)
{
    xbox_input_history_t h={0}; uint8_t p[20]={0,20};
    char held[48],lines[3][48],timing[48];
    p[4]=255; xbox_history_update(&h,p,1000);
    for(int t=1010;t<1300;t+=10) xbox_history_update(&h,p,t);
    assert(h.count==1);
    p[4]=0; xbox_history_update(&h,p,1300);
    p[2]=4; p[6]=255; xbox_history_update(&h,p,1380);
    p[2]=p[6]=0; xbox_history_update(&h,p,1500);
    xbox_history_format(&h,1600,held,lines,timing);
    assert(!strcmp(held,"-") && !strcmp(lines[0],"A > LEFT+X"));
    assert(!strcmp(timing,"LAST 120MS  GAP 80MS"));
    assert(h.events[0].end-h.events[0].start==300);
    xbox_history_format(&h,4500,held,lines,timing);
    assert(!lines[0][0] && !timing[0]);
    /* Rolled chords merge; release/repress stays a separate event. */
    h=(xbox_input_history_t){0}; p[2]=4; xbox_history_update(&h,p,1000);
    p[6]=255; xbox_history_update(&h,p,1020); assert(h.count==1);
    p[2]=p[6]=0; xbox_history_update(&h,p,1040);
    p[6]=255; xbox_history_update(&h,p,1050); assert(h.count==2);
    xbox_history_update(&h,p,9000);
    xbox_history_format(&h,9000,held,lines,timing);
    assert(!strcmp(held,"X") && lines[0][0]);
    /* Release-only transitions don't add phantom combo events. */
    h=(xbox_input_history_t){0}; p[4]=255; xbox_history_update(&h,p,1000);
    p[6]=0; xbox_history_update(&h,p,1200); assert(h.count==1);
    p[4]=0; xbox_history_update(&h,p,1300);
    /* Eight recent events, bounded formatting, oldest first. */
    h=(xbox_input_history_t){0};
    for(int i=0;i<10;i++) {
        p[4]=255; xbox_history_update(&h,p,1000+i*100);
        p[4]=0; xbox_history_update(&h,p,1050+i*100);
    }
    assert(h.count==8 && h.events[0].start==1200);
    xbox_history_format(&h,2000,held,lines,timing);
    assert(!strcmp(lines[0],"A > A > A > A > A > A > A > A"));
    /* Idle expiry resets gaps, and analog center noise does not flood history. */
    xbox_history_update(&h,p,5000); assert(h.count==0);
    p[12]=0x10;p[13]=0x27; xbox_history_update(&h,p,5100); assert(!h.held);
    p[12]=0xc8;p[13]=0x32; xbox_history_update(&h,p,5200); assert(h.held==(1u<<16));
    p[12]=0x10;p[13]=0x27; xbox_history_update(&h,p,5300); assert(h.held==(1u<<16));
    p[12]=0;p[13]=0; xbox_history_update(&h,p,5400); assert(!h.held);
    for(unsigned i=0;i<8;i++) h.events[i]=(xbox_history_event_t){.mask=0xffffff,.start=5500};
    h.count=8;h.held=0xffffff;h.activity=5500;
    xbox_history_format(&h,5600,held,lines,timing);
    for(unsigned i=0;i<3;i++) assert(strlen(lines[i])<48);
    assert(lines[2][0] && strlen(held)<48);
    puts("Input history: sequences, chords, timing, expiry, ring, analog hysteresis PASS");
}
