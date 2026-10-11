/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "xbox_report.h"

int main(void)
{
    uint8_t keys[6] = {0}, report[20];
    xbox_keyboard_report(keys, report);
    for (unsigned i = 0; i < 20; ++i) assert(report[i] == (i == 1 ? 20 : 0));
    const uint8_t buttons[] = {0x52,0x51,0x50,0x4f,0x28,0x2a,0x15,0x17};
    const uint8_t pressure[] = {0x1d,0x1b,0x06,0x19,0x05,0x11,0x14,0x08};
    for (unsigned i = 0; i < 8; ++i) {
        keys[0] = buttons[i];
        xbox_keyboard_report(keys, report);
        assert(report[2] == (1u << i));
        keys[0] = pressure[i];
        xbox_keyboard_report(keys, report);
        assert(report[4+i] == 255 && report[2] == 0);
    }
    const uint8_t pos[] = {0x07,0x1a,0x0f,0x0c};
    const uint8_t neg[] = {0x04,0x16,0x0d,0x0e};
    for (unsigned i = 0; i < 4; ++i) {
        keys[0] = pos[i]; keys[1] = 0;
        xbox_keyboard_report(keys, report);
        assert(report[12+2*i] == 255 && report[13+2*i] == 127);
        keys[0] = neg[i];
        xbox_keyboard_report(keys, report);
        assert(report[12+2*i] == 1 && report[13+2*i] == 128);
        keys[1] = pos[i];
        xbox_keyboard_report(keys, report);
        assert(report[12+2*i] == 0 && report[13+2*i] == 0);
    }
    memset(keys,0,sizeof(keys)); keys[0] = keys[1] = 0x1d;
    xbox_keyboard_report(keys,report);
    assert(report[4] == 255 && report[1] == 20 && report[3] == 0);
    const int16_t analog[4]={12345,-16384,32767,-32767};
    xbox_report_axes(report,analog);
    for(unsigned i=0;i<4;i++) assert((int16_t)((unsigned)report[12+i*2] | ((unsigned)report[13+i*2]<<8))==analog[i]);
    assert(report[4]==255); /* Analog updates preserve the A button. */
    puts("Xbox report encoding: PASS");
}
