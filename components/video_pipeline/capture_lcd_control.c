/* SPDX-License-Identifier: Apache-2.0 */
#include "capture_lcd_control.h"
#include <string.h>

static void (*s_get)(capture_lcd_record_state_t *);
static void (*s_toggle)(void);

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
