// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct record_audio_encoder record_audio_encoder_t;

// One AAC-LC access unit contains this many PCM frames, per channel.
#define RECORD_AUDIO_FRAME_SAMPLES 1024

/**
 * Open an AAC-LC encoder for 16-bit interleaved PCM. Supports the HDMI capture
 * rates (32000, 44100, 48000, 88200, 96000 Hz), with one or two channels.
 * Uses 80 kbit/s per channel and includes ADTS framing. No resampling is done.
 * All calls for one handle belong to a single recorder worker, never an ISR or
 * the I2S producer. Encoding is synchronous; queue PCM before calling it.
 */
esp_err_t record_audio_encoder_open(uint32_t rate, unsigned channels,
                                    record_audio_encoder_t **out);

/**
 * Encode exactly RECORD_AUDIO_FRAME_SAMPLES PCM frames (not scalar samples).
 * The caller owns accumulation and timestamps. The input is copied into an
 * aligned working buffer; no per-frame allocation or producer callback occurs.
 * On success, adts points to one complete ADTS access unit owned by the encoder,
 * valid until its next encode/reset/close call. Copy it before queueing it.
 */
esp_err_t record_audio_encoder_encode(record_audio_encoder_t *encoder,
                                      const int16_t *pcm, size_t frames,
                                      const uint8_t **adts, size_t *length);

/** Discard codec history at a recording discontinuity; not a final-frame flush. */
esp_err_t record_audio_encoder_reset(record_audio_encoder_t *encoder);

/** Release encoder resources. NULL is accepted. */
void record_audio_encoder_close(record_audio_encoder_t *encoder);
