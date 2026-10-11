/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
esp_err_t usb_xbox_init(void);
void usb_xbox_keyboard(const uint8_t keys[6]);
void usb_xbox_touch(const uint8_t keys[6], const int16_t axes[4], bool active);
void usb_xbox_release(void);
/* Validated XID state from the local wireless controller; NULL releases it. */
void usb_xbox_bluetooth(const uint8_t report[20]);
/* Snapshot reports submitted to USB after local/BLE/network arbitration. */
void usb_xbox_history(char held[48],char lines[3][48],char timing[48]);
