// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "esp_err.h"
#include "ts_mux.h"

typedef struct record_audio record_audio_t;
typedef struct {
    uint64_t frames;
    uint64_t silence_frames;
    uint64_t dropped_frames;
} record_audio_stats_t;

/* Establish real samples before advertising an audio track. */
esp_err_t record_audio_open(record_audio_t **out);
/* Start a part on its first video timestamp. Only the writer calls these. */
esp_err_t record_audio_begin(record_audio_t *a, int64_t origin_us);
esp_err_t record_audio_pump(record_audio_t *a, ts_mux_t *mux, int64_t through_us, bool finish);
void record_audio_stats(record_audio_t *a, record_audio_stats_t *out);
void record_audio_close(record_audio_t *a);
