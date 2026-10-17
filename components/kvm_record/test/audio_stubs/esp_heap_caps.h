// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>
#define MALLOC_CAP_SPIRAM 1u
#define MALLOC_CAP_8BIT 2u
void *heap_caps_calloc(size_t count, size_t size, uint32_t caps);
void heap_caps_free(void *ptr);
