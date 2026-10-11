/* SPDX-License-Identifier: Apache-2.0 */
#include "capture_lcd_control.h"
#include <string.h>

static void (*s_get)(capture_lcd_record_state_t *);
static void (*s_toggle)(void);
static void (*s_bt_mode)(bool);
static const char *(*s_bt_status)(void);
static void (*s_bt_get)(capture_lcd_bt_state_t *);
void capture_lcd_bt_register(void (*mode)(bool), const char *(*status)(void),
                            void (*get)(capture_lcd_bt_state_t *))
{ s_bt_mode=mode; s_bt_status=status; s_bt_get=get; }
void capture_lcd_bt_state(capture_lcd_bt_state_t *out)
{ memset(out,0,sizeof(*out)); if(s_bt_get) s_bt_get(out); }
void capture_lcd_bt_mode(bool pairing) { if(s_bt_mode) s_bt_mode(pairing); }
const char *capture_lcd_bt_status(void) { return s_bt_status?s_bt_status():"BLUETOOTH UNAVAILABLE"; }
static void (*s_pad)(const uint8_t[6], const int16_t[4], bool);
void capture_lcd_pad_register(void (*keys)(const uint8_t[6], const int16_t[4], bool)) { s_pad=keys; }
void capture_lcd_pad_keys(const uint8_t keys[6], const int16_t axes[4], bool active)
{ if(s_pad) s_pad(keys,axes,active); }

void capture_lcd_control_register(void (*get)(capture_lcd_record_state_t *),
                                  void (*toggle)(void))
{
    s_get = get;
    s_toggle = toggle;
}

void capture_lcd_record_state(capture_lcd_record_state_t *out)
{
    memset(out, 0, sizeof(*out));
    if (s_get) s_get(out);
}

void capture_lcd_record_toggle(void)
{
    if (s_toggle) s_toggle();
}
