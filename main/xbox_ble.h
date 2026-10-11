/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
void xbox_ble_mode(bool pairing);
const char *xbox_ble_status(void);
void xbox_ble_init(void);
