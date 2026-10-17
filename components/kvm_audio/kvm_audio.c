// SPDX-License-Identifier: Apache-2.0
#include "kvm_audio.h"
#include "sdkconfig.h"

#include <string.h>

#if CONFIG_KVM_ENABLE_AUDIO
#include <math.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define BLOCK_FRAMES (CONFIG_KVM_AUDIO_RATE / 200)
#define DMA_BUFFERS 8
#define STAMP_COUNT 16
#define BROWSER_BLOCKS 8
#define RECORDER_BLOCKS 200

struct kvm_audio_subscription {
    QueueHandle_t queue;
    StaticQueue_t queue_state;
    uint8_t *storage;
    kvm_audio_subscription_stats_t stats;
};

typedef struct {
    int64_t end_us;
    uint64_t frame_index;
    uint32_t epoch;
} dma_stamp_t;

typedef struct {
    union {
        int32_t words[KVM_AUDIO_MAX_FRAMES * KVM_AUDIO_CHANNELS];
        int16_t halfwords[KVM_AUDIO_MAX_FRAMES * KVM_AUDIO_CHANNELS];
    } raw;
    kvm_audio_block_t block;
    kvm_audio_block_t discarded;
} capture_buffers_t;

static const char *TAG = "hdmi_audio";
static StaticSemaphore_t control_state;
static SemaphoreHandle_t control;
static StaticQueue_t stamp_state;
static uint8_t stamp_storage[STAMP_COUNT * sizeof(dma_stamp_t)];
static QueueHandle_t stamps;
static kvm_audio_subscription_t *subscribers[KVM_AUDIO_CONSUMER_COUNT];
static bool initialized;
static esp_err_t init_result = ESP_ERR_INVALID_STATE;
static bool worker_running;
static portMUX_TYPE stats_lock = portMUX_INITIALIZER_UNLOCKED;
static kvm_audio_status_t status;
static uint64_t dma_frame_index;
static uint32_t capture_epoch;

static bool IRAM_ATTR on_receive(i2s_chan_handle_t channel, i2s_event_data_t *event, void *ctx)
{
    (void)channel; (void)event; (void)ctx;
    dma_stamp_t stamp = {.end_us = esp_timer_get_time()};
    portENTER_CRITICAL_ISR(&stats_lock);
    stamp.frame_index = dma_frame_index;
    stamp.epoch = capture_epoch;
    dma_frame_index += BLOCK_FRAMES;
    portEXIT_CRITICAL_ISR(&stats_lock);
    BaseType_t wake = pdFALSE;
    if (xQueueSendFromISR(stamps, &stamp, &wake) != pdTRUE) {
        portENTER_CRITICAL_ISR(&stats_lock);
        status.overruns++;
        portEXIT_CRITICAL_ISR(&stats_lock);
    }
    return wake == pdTRUE;
}

static bool IRAM_ATTR on_overflow(i2s_chan_handle_t channel, i2s_event_data_t *event, void *ctx)
{
    (void)channel; (void)event; (void)ctx;
    portENTER_CRITICAL_ISR(&stats_lock);
    status.overruns++;
    portEXIT_CRITICAL_ISR(&stats_lock);
    return false;
}

static esp_err_t open_receiver(i2s_chan_handle_t *out)
{
    *out = NULL;
    if (CONFIG_KVM_AUDIO_SLOT_BITS != 16 && CONFIG_KVM_AUDIO_SLOT_BITS != 32) return ESP_ERR_INVALID_ARG;
    if (CONFIG_KVM_AUDIO_RATE != 32000 && CONFIG_KVM_AUDIO_RATE != 44100 &&
        CONFIG_KVM_AUDIO_RATE != 48000 && CONFIG_KVM_AUDIO_RATE != 88200 &&
        CONFIG_KVM_AUDIO_RATE != 96000) return ESP_ERR_INVALID_ARG;
    if (!GPIO_IS_VALID_GPIO(CONFIG_KVM_AUDIO_BCLK) || !GPIO_IS_VALID_GPIO(CONFIG_KVM_AUDIO_WS) ||
        !GPIO_IS_VALID_GPIO(CONFIG_KVM_AUDIO_DIN) || CONFIG_KVM_AUDIO_BCLK == CONFIG_KVM_AUDIO_WS ||
        CONFIG_KVM_AUDIO_DIN == CONFIG_KVM_AUDIO_WS || CONFIG_KVM_AUDIO_DIN == CONFIG_KVM_AUDIO_BCLK)
        return ESP_ERR_INVALID_ARG;
    gpio_config_t inputs = {
        .pin_bit_mask = (1ULL << CONFIG_KVM_AUDIO_BCLK) | (1ULL << CONFIG_KVM_AUDIO_WS) |
                        (1ULL << CONFIG_KVM_AUDIO_DIN),
        .mode = GPIO_MODE_INPUT,
    };
    esp_err_t err = gpio_config(&inputs);
    if (err != ESP_OK) return err;
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_SLAVE);
    channel.dma_desc_num = DMA_BUFFERS;
    channel.dma_frame_num = BLOCK_FRAMES;
    err = i2s_new_channel(&channel, NULL, out);
    if (err != ESP_OK) return err;
    i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_KVM_AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(CONFIG_KVM_AUDIO_SLOT_BITS, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = CONFIG_KVM_AUDIO_BCLK,
                     .ws = CONFIG_KVM_AUDIO_WS, .dout = I2S_GPIO_UNUSED, .din = CONFIG_KVM_AUDIO_DIN},
    };
    err = i2s_channel_init_std_mode(*out, &config);
    if (err == ESP_OK) {
        i2s_event_callbacks_t callbacks = {.on_recv = on_receive, .on_recv_q_ovf = on_overflow};
        err = i2s_channel_register_event_callback(*out, &callbacks, NULL);
    }
    if (err != ESP_OK) {
        i2s_del_channel(*out);
        *out = NULL;
    }
    return err;
}

// Caller has stopped I2S, so neither the IDF data queue nor our matching stamp
// queue can advance. Each epoch preserves sample counts without hiding gaps.
static void begin_epoch(uint32_t *handled_overruns)
{
    xQueueReset(stamps);
    portENTER_CRITICAL(&stats_lock);
    capture_epoch++;
    dma_frame_index = 0;
    *handled_overruns = status.overruns;
    portEXIT_CRITICAL(&stats_lock);
}

static bool wanted_locked(void)
{
    for (int i = 0; i < KVM_AUDIO_CONSUMER_COUNT; i++) {
        if (subscribers[i] && subscribers[i]->stats.terminal_error == ESP_OK) return true;
    }
    return false;
}

static bool finish_worker(i2s_chan_handle_t rx, bool running, capture_buffers_t *buffers,
                          esp_err_t error, bool only_if_idle)
{
    // Serialize teardown against a new subscriber starting another producer.
    xSemaphoreTake(control, portMAX_DELAY);
    if (only_if_idle && wanted_locked()) {
        xSemaphoreGive(control);
        return false;
    }
    if (running) i2s_channel_disable(rx);
    if (rx) i2s_del_channel(rx);
    for (int i = 0; i < KVM_AUDIO_CONSUMER_COUNT; i++) {
        if (subscribers[i] && subscribers[i]->stats.terminal_error == ESP_OK)
            subscribers[i]->stats.terminal_error = error;
    }
    heap_caps_free(buffers);
    portENTER_CRITICAL(&stats_lock);
    status.capturing = false;
    portEXIT_CRITICAL(&stats_lock);
    worker_running = false;
    xSemaphoreGive(control);
    return true;
}

static void capture_task(void *arg)
{
    (void)arg;
    i2s_chan_handle_t rx = NULL;
    bool running = false;
    capture_buffers_t *buffers = heap_caps_calloc(1, sizeof(*buffers), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    esp_err_t error = buffers ? open_receiver(&rx) : ESP_ERR_NO_MEM;
    uint32_t handled_overruns = 0;
    if (error == ESP_OK) {
        begin_epoch(&handled_overruns);
        error = i2s_channel_enable(rx);
        running = error == ESP_OK;
        portENTER_CRITICAL(&stats_lock);
        status.capturing = running;
        portEXIT_CRITICAL(&stats_lock);
    }
    while (error == ESP_OK) {
        xSemaphoreTake(control, portMAX_DELAY);
        bool wanted = wanted_locked();
        xSemaphoreGive(control);
        if (!wanted) {
            if (finish_worker(rx, running, buffers, ESP_ERR_INVALID_STATE, true)) {
                vTaskDelete(NULL);
                return;
            }
            continue;
        }

        const size_t bytes = BLOCK_FRAMES * KVM_AUDIO_CHANNELS * (CONFIG_KVM_AUDIO_SLOT_BITS / 8);
        size_t received = 0;
        error = i2s_channel_read(rx, &buffers->raw, bytes, &received, 500);
        if (error != ESP_OK || received != bytes) {
            if (error == ESP_OK) error = ESP_ERR_INVALID_SIZE;
            ESP_LOGW(TAG, "I2S read %s, received=%u requested=%u", esp_err_to_name(error),
                     (unsigned)received, (unsigned)bytes);
            break;
        }
        dma_stamp_t stamp;
        bool have_stamp = xQueueReceive(stamps, &stamp, 0) == pdTRUE;
        portENTER_CRITICAL(&stats_lock);
        uint32_t overruns = status.overruns;
        portEXIT_CRITICAL(&stats_lock);
        if (!have_stamp || overruns != handled_overruns) {
            // An overflow makes timestamp/data association uncertain. Flush both
            // queues together; the recorder sees an epoch change and wall-time gap.
            error = i2s_channel_disable(rx);
            if (error != ESP_OK) break;
            running = false;
            begin_epoch(&handled_overruns);
            error = i2s_channel_enable(rx);
            running = error == ESP_OK;
            continue;
        }
        kvm_audio_block_t *block = &buffers->block;
        block->frames = BLOCK_FRAMES;
        block->frame_index = stamp.frame_index;
        block->epoch = stamp.epoch;
        block->start_us = stamp.end_us - (int64_t)BLOCK_FRAMES * 1000000 / CONFIG_KVM_AUDIO_RATE;
        uint64_t energy = 0;
        int32_t maximum = 0;
        uint32_t active = 0;
        for (unsigned i = 0; i < BLOCK_FRAMES * KVM_AUDIO_CHANNELS; i++) {
            int16_t sample = CONFIG_KVM_AUDIO_SLOT_BITS == 32
                                 ? (int16_t)(buffers->raw.words[i] >> 16) : buffers->raw.halfwords[i];
            block->pcm[i] = sample;
            int32_t magnitude = abs((int32_t)sample);
            if (magnitude > maximum) maximum = magnitude;
            energy += (int64_t)sample * sample;
        }
        for (unsigned i = 0; i < BLOCK_FRAMES * KVM_AUDIO_CHANNELS; i += 2) {
            if (abs(block->pcm[i]) > 32 || abs(block->pcm[i + 1]) > 32) active++;
        }
        float peak = maximum / 32768.0f;
        float rms = sqrtf((float)energy / (BLOCK_FRAMES * KVM_AUDIO_CHANNELS)) / 32768.0f;
        portENTER_CRITICAL(&stats_lock);
        status.frames += BLOCK_FRAMES;
        status.active_frames += active;
        status.peak = peak;
        status.rms = rms;
        if (peak > status.peak_max) status.peak_max = peak;
        status.last_samples_us = stamp.end_us;
        portEXIT_CRITICAL(&stats_lock);

        xSemaphoreTake(control, portMAX_DELAY);
        for (int i = 0; i < KVM_AUDIO_CONSUMER_COUNT; i++) {
            kvm_audio_subscription_t *sub = subscribers[i];
            if (!sub || sub->stats.terminal_error != ESP_OK) continue;
            if (xQueueSend(sub->queue, block, 0) != pdTRUE) {
                // Copying into independent queues never waits for network or SD.
                // Keep recent sound; frame_index exposes exactly what was skipped.
                if (xQueueReceive(sub->queue, &buffers->discarded, 0) == pdTRUE)
                    sub->stats.dropped_frames += buffers->discarded.frames;
                if (xQueueSend(sub->queue, block, 0) != pdTRUE)
                    sub->stats.dropped_frames += block->frames;
            }
        }
        xSemaphoreGive(control);
    }
    // Timeout is reserved for an active reader's bounded wait. Missing source
    // clocks terminate capture with NOT_FOUND, which callers can distinguish.
    if (error == ESP_ERR_TIMEOUT) error = ESP_ERR_NOT_FOUND;
    finish_worker(rx, running, buffers, error == ESP_OK ? ESP_ERR_INVALID_STATE : error, false);
    vTaskDelete(NULL);
}

esp_err_t kvm_audio_init(void)
{
    if (initialized) return init_result;
    control = xSemaphoreCreateMutexStatic(&control_state);
    stamps = xQueueCreateStatic(STAMP_COUNT, sizeof(dma_stamp_t), stamp_storage, &stamp_state);
    i2s_chan_handle_t rx = NULL;
    init_result = open_receiver(&rx);
    if (rx) i2s_del_channel(rx);
    status.rate = CONFIG_KVM_AUDIO_RATE;
    initialized = true;
    return init_result;
}

uint32_t kvm_audio_rate(void) { return CONFIG_KVM_AUDIO_RATE; }

esp_err_t kvm_audio_subscribe(kvm_audio_consumer_t kind, kvm_audio_subscription_t **out)
{
    if (!out || kind < 0 || kind >= KVM_AUDIO_CONSUMER_COUNT) return ESP_ERR_INVALID_ARG;
    *out = NULL;
    if (!initialized || init_result != ESP_OK) return init_result;
    const unsigned blocks = kind == KVM_AUDIO_RECORDER ? RECORDER_BLOCKS : BROWSER_BLOCKS;
    kvm_audio_subscription_t *sub = heap_caps_calloc(1, sizeof(*sub), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!sub) return ESP_ERR_NO_MEM;
    sub->storage = heap_caps_malloc(blocks * sizeof(kvm_audio_block_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!sub->storage) { heap_caps_free(sub); return ESP_ERR_NO_MEM; }
    sub->queue = xQueueCreateStatic(blocks, sizeof(kvm_audio_block_t), sub->storage, &sub->queue_state);
    esp_err_t error = ESP_OK;
    xSemaphoreTake(control, portMAX_DELAY);
    if (subscribers[kind]) {
        error = ESP_ERR_INVALID_STATE;
    } else {
        subscribers[kind] = sub;
        if (!worker_running) {
            portENTER_CRITICAL(&stats_lock);
            status = (kvm_audio_status_t){.rate = CONFIG_KVM_AUDIO_RATE};
            portEXIT_CRITICAL(&stats_lock);
            worker_running = true;
            if (xTaskCreate(capture_task, "audio_capture", 6144, NULL, 6, NULL) != pdPASS) {
                worker_running = false;
                subscribers[kind] = NULL;
                error = ESP_ERR_NO_MEM;
            }
        }
    }
    xSemaphoreGive(control);
    if (error != ESP_OK) {
        vQueueDelete(sub->queue);
        heap_caps_free(sub->storage);
        heap_caps_free(sub);
        return error;
    }
    *out = sub;
    return ESP_OK;
}

esp_err_t kvm_audio_read(kvm_audio_subscription_t *sub, kvm_audio_block_t *block,
                         uint32_t timeout_ms)
{
    if (!sub || !block) return ESP_ERR_INVALID_ARG;
    if (xQueueReceive(sub->queue, block, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        xSemaphoreTake(control, portMAX_DELAY);
        sub->stats.received_frames += block->frames;
        xSemaphoreGive(control);
        return ESP_OK;
    }
    xSemaphoreTake(control, portMAX_DELAY);
    esp_err_t error = sub->stats.terminal_error;
    xSemaphoreGive(control);
    return error == ESP_OK ? ESP_ERR_TIMEOUT : error;
}

void kvm_audio_subscription_stats(kvm_audio_subscription_t *sub,
                                  kvm_audio_subscription_stats_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!sub) return;
    xSemaphoreTake(control, portMAX_DELAY);
    *out = sub->stats;
    xSemaphoreGive(control);
}

void kvm_audio_unsubscribe(kvm_audio_subscription_t *sub)
{
    if (!sub) return;
    xSemaphoreTake(control, portMAX_DELAY);
    for (int i = 0; i < KVM_AUDIO_CONSUMER_COUNT; i++) {
        if (subscribers[i] == sub) subscribers[i] = NULL;
    }
    xSemaphoreGive(control);
    vQueueDelete(sub->queue);
    heap_caps_free(sub->storage);
    heap_caps_free(sub);
}

void kvm_audio_get_status(kvm_audio_status_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&stats_lock);
    *out = status;
    portEXIT_CRITICAL(&stats_lock);
    if (!initialized) return;
    xSemaphoreTake(control, portMAX_DELAY);
    out->browser = subscribers[KVM_AUDIO_BROWSER] != NULL;
    out->recorder = subscribers[KVM_AUDIO_RECORDER] != NULL;
    xSemaphoreGive(control);
}

#else
esp_err_t kvm_audio_init(void) { return ESP_ERR_NOT_SUPPORTED; }
uint32_t kvm_audio_rate(void) { return 0; }
esp_err_t kvm_audio_subscribe(kvm_audio_consumer_t kind, kvm_audio_subscription_t **out)
{
    (void)kind;
    if (out) *out = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t kvm_audio_read(kvm_audio_subscription_t *sub, kvm_audio_block_t *block, uint32_t timeout_ms)
{
    (void)sub; (void)block; (void)timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
}
void kvm_audio_subscription_stats(kvm_audio_subscription_t *sub, kvm_audio_subscription_stats_t *out)
{
    (void)sub;
    if (out) *out = (kvm_audio_subscription_stats_t){.terminal_error = ESP_ERR_NOT_SUPPORTED};
}
void kvm_audio_unsubscribe(kvm_audio_subscription_t *sub) { (void)sub; }
void kvm_audio_get_status(kvm_audio_status_t *out) { if (out) memset(out, 0, sizeof(*out)); }
#endif
