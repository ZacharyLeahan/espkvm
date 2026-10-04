/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool available;
    bool recording;
    bool busy;
    bool local_consumer;
    uint32_t seconds;
    char message[32];
} capture_lcd_record_state_t;

/* Register before capture starts. The application owns the recorder, avoiding
 * a dependency cycle between video_pipeline and kvm_record. */
void capture_lcd_control_register(void (*get)(capture_lcd_record_state_t *),
                                  void (*toggle)(void));
void capture_lcd_record_state(capture_lcd_record_state_t *out);
void capture_lcd_record_toggle(void);
