/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { XBOX_HISTORY_COUNT=8, XBOX_HISTORY_IDLE_MS=3000 };
typedef struct { uint32_t mask; int64_t start,end,gap; } xbox_history_event_t;
typedef struct {
    uint32_t held;
    unsigned count;
    int64_t activity,release;
    xbox_history_event_t events[XBOX_HISTORY_COUNT];
} xbox_input_history_t;

/* Hysteresis keeps stick noise out of the digital direction history. This
 * observes the output only; it never changes the actual analog USB values. */
static inline uint32_t xbox_history_mask(const uint8_t p[20],uint32_t previous)
{
    uint32_t mask=p[2];
    for(unsigned i=0;i<8;i++) if(p[4+i]) mask|=1u<<(8+i);
    for(unsigned i=0;i<4;i++) {
        int v=(int16_t)(p[12+2*i]|((uint16_t)p[13+2*i]<<8));
        unsigned bit=16+2*i;
        if(v>((previous&(1u<<bit))?8000:12000)) mask|=1u<<bit;
        if(v<-((previous&(1u<<(bit+1)))?8000:12000)) mask|=1u<<(bit+1);
    }
    return mask;
}
static inline void xbox_history_update(xbox_input_history_t *h,const uint8_t p[20],int64_t now)
{
    uint32_t mask=xbox_history_mask(p,h->held);
    if(!h->held && h->count && now-h->activity>=XBOX_HISTORY_IDLE_MS) {
        h->count=0; h->release=0;
    }
    if(mask==h->held) { if(mask) h->activity=now; return; }
    uint32_t old=h->held;
    xbox_history_event_t *last=h->count?&h->events[h->count-1]:NULL;
    /* Merge a chord rolled on within 50 ms, but never merge repeated taps
     * separated by a release or unrelated sequential directions. */
    bool merge=old && last && last->mask==old && (mask&old)==old &&
        now-last->start<=50;
    if(last && old && !last->end && !merge) last->end=now;
    if(mask & ~old) {
        if(merge) last->mask=mask;
        else {
            if(h->count==XBOX_HISTORY_COUNT) {
                memmove(h->events,h->events+1,(XBOX_HISTORY_COUNT-1)*sizeof(*last));
                --h->count;
            }
            int64_t gap=old || !h->count ? 0 : now-h->release;
            h->events[h->count++]=(xbox_history_event_t){.mask=mask,.start=now,.gap=gap};
        }
    }
    if(!mask) h->release=now;
    h->held=mask; h->activity=now;
}
static inline void xbox_history_label(uint32_t mask,char *out,size_t size)
{
    static const char *names[]={"UP","DN","LEFT","RIGHT","START","BACK","L3","R3",
        "A","B","X","Y","RB","LB","LT","RT",
        "LS-R","LS-L","LS-U","LS-D","RS-R","RS-L","RS-U","RS-D"};
    size_t used=0;
    if(!size) return;
    out[0]=0;
    for(unsigned i=0;i<24;i++) if(mask&(1u<<i)) {
        int n=snprintf(out+used,size-used,"%s%s",used?"+":"",names[i]);
        if(n<0) break;
        if((size_t)n>=size-used) {
            if(size>=4) memcpy(out+size-4,"...",4);
            break;
        }
        used+=(size_t)n;
    }
    if(!mask) snprintf(out,size,"-");
}
static inline void xbox_history_format(const xbox_input_history_t *h,int64_t now,
                                      char held[48],char lines[3][48],char timing[48])
{
    memset(lines,0,3*48); timing[0]=0;
    xbox_history_label(h->held,held,48);
    if(!h->count || (!h->held && now-h->activity>=XBOX_HISTORY_IDLE_MS)) return;
    unsigned row=0;
    for(unsigned i=0;i<h->count;i++) {
        char label[13]; xbox_history_label(h->events[i].mask,label,sizeof(label));
        const char *sep=i?" > ":"";
        size_t used=strlen(lines[row]);
        if(used+strlen(sep)+strlen(label)>=48 && ++row>=3) break;
        used=strlen(lines[row]);
        snprintf(lines[row]+used,48-used,"%s%s",sep,label);
    }
    const xbox_history_event_t *e=&h->events[h->count-1];
    snprintf(timing,48,"LAST %lldMS  GAP %lldMS",(long long)((e->end?e->end:now)-e->start),(long long)e->gap);
}
