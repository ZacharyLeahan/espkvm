/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "xbox_report.h"
/* Show translated Xbox buttons (LB/RB retain the Series labels). */
static inline bool xbox_ble_buttons(const uint8_t p[20],char *text,size_t size)
{
    static const char *labels[]={"UP","DOWN","LEFT","RIGHT","START","BACK","L3","R3",
        "A","B","X","Y","RB","LB","LT","RT"};
    unsigned mask=p[2];
    for(unsigned i=0;i<8;i++) if(p[4+i]) mask|=1u<<(8+i);
    size_t used=0;
    if(size) text[0]=0;
    for(unsigned i=0;i<16;i++) if(mask&(1u<<i)) {
        if(used+1>=size) break;
        int n=snprintf(text+used,size-used,"%s%s",used?" + ":"",labels[i]);
        if(n<0 || (size_t)n>=size-used) break;
        used+=(size_t)n;
    }
    return mask!=0;
}
/* Series BLE report ID 1, 16-byte payload (ID is in the GATT descriptor).
 * Protocol reference: github.com/rear16/ESP32-XboxBLE, Discovered Mapping.
 * Reject other layouts rather than guessing and sending unintended buttons. */
static inline bool xbox_ble_report(const uint8_t *p,size_t n,uint8_t out[20])
{
    memset(out,0,20); out[1]=20;
    if(n!=16 || p[12]>8) return false;
    unsigned lt=p[8]|(p[9]<<8),rt=p[10]|(p[11]<<8);
    if(lt>1023 || rt>1023) return false;
    static const uint8_t hats[]={0,1,9,8,10,2,6,4,5};
    out[2]=hats[p[12]] | ((p[14]&8)?16:0) | ((p[14]&4)?32:0) |
        ((p[14]&32)?64:0) | ((p[14]&64)?128:0);
    out[4]=(p[13]&1)?255:0; out[5]=(p[13]&2)?255:0;
    out[6]=(p[13]&8)?255:0; out[7]=(p[13]&16)?255:0;
    out[8]=(p[13]&128)?255:0; /* RB -> Black */
    out[9]=(p[13]&64)?255:0;  /* LB -> White */
    out[10]=(lt*255+511)/1023; out[11]=(rt*255+511)/1023;
    int16_t axes[4];
    for(unsigned i=0;i<4;i++) {
        int v=(p[2*i]|(p[2*i+1]<<8))-32768;
        if(i&1) v=-v;
        if(v>32767) v=32767;
        axes[i]=v;
    }
    xbox_report_axes(out,axes);
    return true;
}
