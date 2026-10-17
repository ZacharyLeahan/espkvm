// SPDX-License-Identifier: Apache-2.0
// Exercise the real recording timeline; codec/queue substitutes expose its PCM
// and PTS so timing regressions are observable without ESP hardware or AAC loss.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "record_audio.h"
#include "record_audio_encoder.h"
#include "kvm_audio.h"

#define ORIGIN_US 1000000
#define MAX_BLOCKS 32
#define MAX_ENCODED 8
struct kvm_audio_subscription { int unused; };
struct record_audio_encoder { int unused; };
static struct kvm_audio_subscription subscription;
static struct record_audio_encoder encoder;
static kvm_audio_block_t blocks[MAX_BLOCKS];
static size_t block_count, next_block, encoded_count, muxed_count;
static int16_t encoded[MAX_ENCODED][RECORD_AUDIO_FRAME_SAMPLES * 2];
static uint64_t timestamps[MAX_ENCODED];
static uint64_t dropped;
static unsigned resets, closes, unsubscribes;
static bool encode_fails, mux_fails;
static esp_err_t empty_result;

void *heap_caps_calloc(size_t n, size_t bytes, uint32_t caps)
{ (void)caps; return calloc(n, bytes); }
void heap_caps_free(void *p) { free(p); }
uint32_t kvm_audio_rate(void) { return 48000; }
esp_err_t kvm_audio_subscribe(kvm_audio_consumer_t kind, kvm_audio_subscription_t **out)
{ assert(kind == KVM_AUDIO_RECORDER); *out = &subscription; return ESP_OK; }
void kvm_audio_unsubscribe(kvm_audio_subscription_t *s)
{ if (s) { assert(s == &subscription); unsubscribes++; } }
esp_err_t kvm_audio_read(kvm_audio_subscription_t *s, kvm_audio_block_t *out, uint32_t ms)
{
    assert(s == &subscription);
    assert(ms == 0 || ms == 30 || ms == 600);
    if (next_block == block_count) return empty_result;
    *out = blocks[next_block++];
    return ESP_OK;
}
void kvm_audio_subscription_stats(kvm_audio_subscription_t *s, kvm_audio_subscription_stats_t *out)
{ assert(s == &subscription); memset(out, 0, sizeof(*out)); out->dropped_frames = dropped; }
esp_err_t record_audio_encoder_open(uint32_t rate, unsigned channels, record_audio_encoder_t **out)
{ assert(rate == 48000 && channels == 2); *out = &encoder; return ESP_OK; }
void record_audio_encoder_close(record_audio_encoder_t *e)
{ if (e) { assert(e == &encoder); closes++; } }
esp_err_t record_audio_encoder_reset(record_audio_encoder_t *e)
{ assert(e == &encoder); resets++; return ESP_OK; }
esp_err_t record_audio_encoder_encode(record_audio_encoder_t *e, const int16_t *pcm,
                                     size_t frames, const uint8_t **out, size_t *length)
{
    static const uint8_t fake_adts[] = {1};
    assert(e == &encoder && frames == RECORD_AUDIO_FRAME_SAMPLES);
    if (encode_fails) return ESP_FAIL;
    assert(encoded_count < MAX_ENCODED);
    memcpy(encoded[encoded_count++], pcm, sizeof(encoded[0]));
    *out = fake_adts; *length = sizeof(fake_adts);
    return ESP_OK;
}
bool ts_mux_audio_adts(ts_mux_t *mux, const uint8_t *data, size_t size, uint64_t pts)
{
    (void)mux;
    assert(data && size == 1);
    if (mux_fails) return false;
    assert(muxed_count < MAX_ENCODED);
    timestamps[muxed_count++] = pts;
    return true;
}

static void clear(void)
{
    memset(blocks, 0, sizeof(blocks));
    memset(encoded, 0, sizeof(encoded));
    block_count = next_block = encoded_count = muxed_count = 0;
    resets = closes = unsubscribes = 0;
    dropped = 0; encode_fails = mux_fails = false; empty_result = ESP_ERR_TIMEOUT;
}
static void block(uint32_t epoch, uint64_t index, int64_t start_us, int value_base)
{
    assert(block_count < MAX_BLOCKS);
    kvm_audio_block_t *b = &blocks[block_count++];
    b->epoch = epoch; b->frame_index = index; b->start_us = start_us; b->frames = 240;
    for (unsigned i = 0; i < b->frames; i++) {
        b->pcm[i * 2] = value_base + (int)i;
        b->pcm[i * 2 + 1] = -(value_base + (int)i);
    }
}
static record_audio_t *open_audio(void)
{
    record_audio_t *a = NULL;
    assert(record_audio_open(&a) == ESP_OK && a);
    assert(record_audio_begin(a, ORIGIN_US) == ESP_OK);
    return a;
}
static void sample(size_t frame, size_t offset, int value)
{
    assert(encoded[frame][offset * 2] == value);
    assert(encoded[frame][offset * 2 + 1] == -value);
}

static void test_initial_trim_and_padding(void)
{
    clear();
    for (int i = 0; i < 5; i++) block(1, i * 240, ORIGIN_US - 2500 + i * 5000, 1 + i * 240);
    record_audio_t *a = open_audio();
    assert(record_audio_pump(a, NULL, ORIGIN_US + 20000, false) == ESP_OK);
    assert(encoded_count == 0);
    assert(record_audio_pump(a, NULL, ORIGIN_US + 20000, true) == ESP_OK);
    assert(encoded_count == 1 && timestamps[0] == 0);
    for (size_t i = 0; i < 960; i++) sample(0, i, i + 121);
    for (size_t i = 960; i < 1024; i++) sample(0, i, 0);
    record_audio_stats_t stats; record_audio_stats(a, &stats);
    assert(stats.frames == 1024 && stats.silence_frames == 64);
    record_audio_close(a); assert(closes == 1 && unsubscribes == 1);
}

static void test_drop_preserves_clock_and_ignores_timestamp_jitter(void)
{
    clear();
    block(1, 0, ORIGIN_US, 1);
    block(1, 480, ORIGIN_US + 17000, 481); // Sample index, not scheduler jitter, determines its position.
    block(1, 720, ORIGIN_US + 18000, 721);
    block(1, 960, ORIGIN_US + 26000, 961);
    dropped = 240;
    record_audio_t *a = open_audio();
    assert(record_audio_pump(a, NULL, ORIGIN_US + 21334, false) == ESP_OK);
    assert(encoded_count == 1 && timestamps[0] == 0);
    for (size_t i = 0; i < 1024; i++) sample(0, i, i >= 240 && i < 480 ? 0 : i + 1);
    record_audio_stats_t stats; record_audio_stats(a, &stats);
    assert(stats.silence_frames == 240 && stats.dropped_frames == 240);
    record_audio_close(a);
}

static void test_epoch_reset_and_late_start(void)
{
    clear();
    block(1, 0, ORIGIN_US + 5000, 1); // 240 frames of leading silence.
    block(2, 0, ORIGIN_US + 15000, 2001); // Reset starts 240 frames after the old epoch ends.
    block(2, 240, ORIGIN_US + 30000, 2241);
    record_audio_t *a = open_audio();
    assert(record_audio_pump(a, NULL, ORIGIN_US + 21334, false) == ESP_OK);
    for (size_t i = 0; i < 240; i++) sample(0, i, 0);
    for (size_t i = 240; i < 480; i++) sample(0, i, i - 239);
    for (size_t i = 480; i < 720; i++) sample(0, i, 0);
    for (size_t i = 720; i < 1024; i++) sample(0, i, i + 1281);
    record_audio_close(a);
}

static void test_split_keeps_partly_consumed_block(void)
{
    clear();
    for (int i = 0; i < 9; i++) block(1, i * 240, ORIGIN_US + i * 5000, 1 + i * 240);
    record_audio_t *a = open_audio();
    assert(record_audio_pump(a, NULL, ORIGIN_US + 12000, true) == ESP_OK); // Split after 576 frames, within block 3.
    assert(encoded_count == 1);
    for (size_t i = 0; i < 576; i++) sample(0, i, i + 1);
    for (size_t i = 576; i < 1024; i++) sample(0, i, 0);
    assert(record_audio_begin(a, ORIGIN_US + 12000) == ESP_OK);
    assert(record_audio_pump(a, NULL, ORIGIN_US + 33334, false) == ESP_OK);
    assert(encoded_count == 2 && resets == 2);
    assert(timestamps[0] == 0 && timestamps[1] == 0);
    for (size_t i = 0; i < 1024; i++) sample(1, i, i + 577);
    record_audio_close(a);
}

static void test_errors_are_not_silent_success(void)
{
    clear();
    record_audio_t *a = NULL;
    assert(record_audio_open(&a) == ESP_ERR_TIMEOUT && !a);
    assert(closes == 1 && unsubscribes == 1);
    for (unsigned mode = 0; mode < 3; mode++) {
        clear();
        for (int i = 0; i < 5; i++) block(1, i * 240, ORIGIN_US + i * 5000, i * 240 + 1);
        a = open_audio();
        encode_fails = mode == 0; mux_fails = mode == 1;
        if (mode == 2) { block_count = 1; empty_result = ESP_FAIL; }
        assert(record_audio_pump(a, NULL, ORIGIN_US + 25000, false) != ESP_OK);
        record_audio_close(a);
    }
}

int main(void)
{
    test_initial_trim_and_padding();
    test_drop_preserves_clock_and_ignores_timestamp_jitter();
    test_epoch_reset_and_late_start();
    test_split_keeps_partly_consumed_block();
    test_errors_are_not_silent_success();
    puts("record audio: trim, padding, loss, epoch, split and failure cases passed");
}
