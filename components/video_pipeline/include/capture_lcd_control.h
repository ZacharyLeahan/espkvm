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
/* Local pad callback, installed before capture; no USB dependency in video. */
void capture_lcd_pad_register(void (*keys)(const uint8_t[6], const int16_t[4], bool));
void capture_lcd_pad_keys(const uint8_t keys[6], const int16_t axes[4], bool active);
typedef struct { bool enabled; char buttons[48]; } capture_lcd_bt_state_t;
void capture_lcd_bt_register(void (*mode)(bool), const char *(*status)(void),
                            void (*get)(capture_lcd_bt_state_t *));
void capture_lcd_bt_state(capture_lcd_bt_state_t *out);
void capture_lcd_bt_mode(bool pairing);
const char *capture_lcd_bt_status(void);
