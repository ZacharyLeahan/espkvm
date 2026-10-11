/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
esp_err_t usb_xbox_init(void);
void usb_xbox_keyboard(const uint8_t keys[6]);
void usb_xbox_touch(const uint8_t keys[6], const int16_t axes[4], bool active);
void usb_xbox_release(void);
