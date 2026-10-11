/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
#include <string.h>
static inline void xbox_report_axes(uint8_t out[20], const int16_t axes[4])
{
    for (unsigned i=0;i<4;i++) {
        uint16_t v=(uint16_t)axes[i];
        out[12+2*i]=v & 255;
        out[13+2*i]=v >> 8;
    }
}

/* Original Xbox XID wire report: digital mask, eight pressure values,
 * then four signed little-endian stick axes. No native struct padding. */
static inline void xbox_keyboard_report(const uint8_t keys[6], uint8_t out[20])
{
    memset(out, 0, 20);
    out[1] = 20;
    int lx = 0, ly = 0, rx = 0, ry = 0;
    uint8_t held[256] = {0};
    for (unsigned i = 0; i < 6; ++i) held[keys[i]] = 1;
    /* HID keyboard usage numbers; opposite directions cancel. */
    out[2] = held[0x52] | (held[0x51] << 1) | (held[0x50] << 2) |
             (held[0x4f] << 3) | (held[0x28] << 4) | (held[0x2a] << 5) |
             (held[0x15] << 6) | (held[0x17] << 7);
    const uint8_t pressure_keys[8] = {0x1d,0x1b,0x06,0x19,0x05,0x11,0x14,0x08};
    for (unsigned i = 0; i < 8; ++i) out[4+i] = held[pressure_keys[i]] ? 255 : 0;
    lx = (held[0x07] - held[0x04]) * 32767; /* D - A */
    ly = (held[0x1a] - held[0x16]) * 32767; /* W - S */
    rx = (held[0x0f] - held[0x0d]) * 32767; /* L - J */
    ry = (held[0x0c] - held[0x0e]) * 32767; /* I - K */
    const int16_t axes[4] = {lx, ly, rx, ry};
    xbox_report_axes(out,axes);
}
