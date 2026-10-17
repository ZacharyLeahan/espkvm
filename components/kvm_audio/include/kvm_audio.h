// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KVM_AUDIO_CHANNELS 2
#define KVM_AUDIO_MAX_FRAMES 480

typedef enum {
    KVM_AUDIO_BROWSER,
    KVM_AUDIO_RECORDER,
    KVM_AUDIO_CONSUMER_COUNT,
} kvm_audio_consumer_t;

typedef struct {
    // First sample's absolute esp_timer time, estimated from DMA completion.
    int64_t start_us;
    // Frame index within epoch. Queue drops leave gaps; a receiver reset starts
    // a new epoch at zero. Use start_us to account for time across epochs.
    uint64_t frame_index;
    uint32_t epoch;
    uint16_t frames;
    int16_t pcm[KVM_AUDIO_MAX_FRAMES * KVM_AUDIO_CHANNELS];
} kvm_audio_block_t;

typedef struct kvm_audio_subscription kvm_audio_subscription_t;

typedef struct {
    uint64_t received_frames;
    uint64_t dropped_frames;
    esp_err_t terminal_error;
} kvm_audio_subscription_stats_t;

typedef struct {
    bool capturing;
    bool browser;
    bool recorder;
    uint32_t rate;
    uint64_t frames;
    uint64_t active_frames;
    uint32_t overruns;
    float peak;
    float rms;
    float peak_max;
    int64_t last_samples_us;
} kvm_audio_status_t;

// Called once during startup; probes configuration, not clock/sample presence.
esp_err_t kvm_audio_init(void);
uint32_t kvm_audio_rate(void);

// One subscription per kind. The producer starts lazily and is shared by both.
// Call read with a bounded timeout to establish actual sample presence.
esp_err_t kvm_audio_subscribe(kvm_audio_consumer_t kind, kvm_audio_subscription_t **out);

// A subscription has one reader. Do not read and unsubscribe it concurrently.
// ESP_ERR_TIMEOUT means still active, but no block arrived within timeout_ms.
// Capture errors become terminal after already-queued blocks are drained.
esp_err_t kvm_audio_read(kvm_audio_subscription_t *sub, kvm_audio_block_t *block,
                         uint32_t timeout_ms);
void kvm_audio_subscription_stats(kvm_audio_subscription_t *sub,
                                  kvm_audio_subscription_stats_t *out);
void kvm_audio_unsubscribe(kvm_audio_subscription_t *sub);
void kvm_audio_get_status(kvm_audio_status_t *out);

#ifdef __cplusplus
}
#endif
