// SPDX-License-Identifier: Apache-2.0
#include "record_audio.h"
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "kvm_audio.h"
#include "record_audio_encoder.h"

struct record_audio {
    kvm_audio_subscription_t *sub;
    record_audio_encoder_t *encoder;
    kvm_audio_block_t block;
    bool pending;
    bool anchored;
    uint32_t epoch;
    uint32_t rate;
    uint64_t anchor_index;
    int64_t anchor_frame;
    int64_t origin_us;
    int64_t cursor;
    size_t used;
    int16_t pcm[RECORD_AUDIO_FRAME_SAMPLES * KVM_AUDIO_CHANNELS];
    record_audio_stats_t stats;
};

void record_audio_close(record_audio_t *a)
{
    if (!a) return;
    kvm_audio_unsubscribe(a->sub);
    record_audio_encoder_close(a->encoder);
    heap_caps_free(a);
}

esp_err_t record_audio_open(record_audio_t **out)
{
    *out = NULL;
    record_audio_t *a = heap_caps_calloc(1, sizeof(*a), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!a) return ESP_ERR_NO_MEM;
    a->rate = kvm_audio_rate();
    esp_err_t err = record_audio_encoder_open(a->rate, KVM_AUDIO_CHANNELS, &a->encoder);
    if (err == ESP_OK) err = kvm_audio_subscribe(KVM_AUDIO_RECORDER, &a->sub);
    if (err == ESP_OK) err = kvm_audio_read(a->sub, &a->block, 600);
    if (err != ESP_OK) {
        record_audio_close(a);
        return err;
    }
    a->pending = true;
    *out = a;
    return ESP_OK;
}

esp_err_t record_audio_begin(record_audio_t *a, int64_t origin_us)
{
    if (!a) return ESP_OK;
    a->origin_us = origin_us;
    a->cursor = 0;
    a->used = 0;
    a->anchored = false;
    return record_audio_encoder_reset(a->encoder);
}

static esp_err_t append(record_audio_t *a, ts_mux_t *mux, const int16_t *pcm, size_t frames)
{
    while (frames) {
        size_t n = RECORD_AUDIO_FRAME_SAMPLES - a->used;
        if (n > frames) n = frames;
        int16_t *dst = a->pcm + a->used * KVM_AUDIO_CHANNELS;
        if (pcm) {
            memcpy(dst, pcm, n * KVM_AUDIO_CHANNELS * sizeof(*pcm));
            pcm += n * KVM_AUDIO_CHANNELS;
        } else {
            memset(dst, 0, n * KVM_AUDIO_CHANNELS * sizeof(*dst));
            a->stats.silence_frames += n;
        }
        a->used += n;
        a->cursor += n;
        frames -= n;
        if (a->used == RECORD_AUDIO_FRAME_SAMPLES) {
            const uint8_t *adts = NULL;
            size_t bytes = 0;
            esp_err_t err = record_audio_encoder_encode(a->encoder, a->pcm, a->used, &adts, &bytes);
            if (err != ESP_OK) return err;
            const uint64_t pts = (uint64_t)(a->cursor - a->used) * 90000u / a->rate;
            if (!ts_mux_audio_adts(mux, adts, bytes, pts)) return ESP_ERR_INVALID_RESPONSE;
            a->stats.frames += a->used;
            a->used = 0;
        }
    }
    return ESP_OK;
}

esp_err_t record_audio_pump(record_audio_t *a, ts_mux_t *mux, int64_t through_us, bool finish)
{
    if (!a || !a->origin_us || through_us < a->origin_us) return ESP_OK;
    const int64_t limit = (through_us - a->origin_us) * a->rate / 1000000;
    /* The sample index removes task/DMA callback jitter within a capture epoch.
     * Absolute timestamps bridge receiver resets and preserve time across loss. */
    while (a->cursor < limit) {
        if (!a->pending) {
            esp_err_t err = kvm_audio_read(a->sub, &a->block, finish ? 30 : 0);
            if (err == ESP_ERR_TIMEOUT) break;
            if (err != ESP_OK) return err;
            a->pending = true;
        }
        if (!a->anchored || a->epoch != a->block.epoch) {
            a->anchored = true;
            a->epoch = a->block.epoch;
            a->anchor_index = a->block.frame_index;
            a->anchor_frame = (a->block.start_us - a->origin_us) * a->rate / 1000000;
        }
        const int64_t first = a->anchor_frame + (int64_t)(a->block.frame_index - a->anchor_index);
        const int64_t last = first + a->block.frames;
        if (last <= a->cursor) {
            a->pending = false;
            continue;
        }
        if (first > a->cursor) {
            int64_t gap = first - a->cursor;
            if (gap > limit - a->cursor) gap = limit - a->cursor;
            esp_err_t err = append(a, mux, NULL, gap);
            if (err != ESP_OK) return err;
            continue;
        }
        size_t offset = a->cursor - first;
        size_t n = a->block.frames - offset;
        if ((int64_t)n > limit - a->cursor) n = limit - a->cursor;
        esp_err_t err = append(a, mux, a->block.pcm + offset * KVM_AUDIO_CHANNELS, n);
        if (err != ESP_OK) return err;
        if (a->cursor >= last) a->pending = false;
    }
    if (finish && a->used) {
        /* AAC is fixed at 1024 samples; pad only the last incomplete frame. */
        return append(a, mux, NULL, RECORD_AUDIO_FRAME_SAMPLES - a->used);
    }
    return ESP_OK;
}

void record_audio_stats(record_audio_t *a, record_audio_stats_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!a) return;
    *out = a->stats;
    kvm_audio_subscription_stats_t capture;
    kvm_audio_subscription_stats(a->sub, &capture);
    out->dropped_frames = capture.dropped_frames;
}
