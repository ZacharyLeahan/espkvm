/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "../main/xbox_ble_report.h"
int main(void)
{
    uint8_t p[16]={0},o[20];
    for(unsigned i=0;i<4;i++) p[2*i+1]=128;
    assert(xbox_ble_report(p,16,o));
    for(unsigned i=2;i<20;i++) assert(!o[i]);
    assert(!xbox_ble_report(p,15,o));
    p[12]=9; assert(!xbox_ble_report(p,16,o)); p[12]=2;
    p[13]=0xdb; p[14]=0x6c;
    p[8]=255;p[9]=3;p[10]=0;p[11]=2;
    assert(xbox_ble_report(p,16,o));
    assert(o[2]==0xf9);
    for(unsigned i=4;i<10;i++) assert(o[i]==255);
    assert(o[10]==255 && o[11]==128);
    p[9]=4;assert(!xbox_ble_report(p,16,o));p[9]=3;
    p[0]=255;p[1]=255;p[2]=0;p[3]=0;
    assert(xbox_ble_report(p,16,o));
    assert(o[12]==255 && o[13]==127 && o[14]==255 && o[15]==127);
    puts("Series BLE to Xbox XID: PASS");
}
