// SPDX-License-Identifier: Apache-2.0
#include "sdkconfig.h"
#include "web_priv.h"
#include "kvm_auth.h"
#include "kvm_caps.h"
#include "http_server.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#if CONFIG_KVM_ENABLE_AUDIO
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "lwip/sockets.h"
#include "lwip/tcp.h"

static atomic_bool busy;
static portMUX_TYPE stats_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t frames, overruns, active_frames;
static float peak, rms, peak_max;
static int64_t last_samples;
static i2s_chan_handle_t rx;
// The one-listener guard owns this reference until its worker has finished.
static kvm_auth_session_ref_t stream_session;
#define AUDIO_SAMPLES ((CONFIG_KVM_AUDIO_RATE / 200) * 2)
#define AUDIO_STACK_BYTES 6144
typedef struct {
    union { int32_t words[AUDIO_SAMPLES]; int16_t halfwords[AUDIO_SAMPLES]; } raw;
    int16_t pcm[AUDIO_SAMPLES];
} audio_buffers_t;

static bool send_all(httpd_req_t *req, const char *data, size_t size)
{
    // The public HTTP send path retains HTTPS's session-specific send override.
    while (size) {
        int sent = httpd_send(req, data, size);
        if (sent <= 0) return false;
        data += sent;
        size -= sent;
    }
    return true;
}

static void log_audio_heap(const char *stage)
{
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
    ESP_LOGI("hdmi_audio", "%s internal DMA heap free=%u largest=%u", stage,
             (unsigned)heap_caps_get_free_size(caps),
             (unsigned)heap_caps_get_largest_free_block(caps));
}

static bool IRAM_ATTR overflow(i2s_chan_handle_t channel, i2s_event_data_t *event, void *ctx)
{
    (void)channel; (void)event; (void)ctx;
    portENTER_CRITICAL_ISR(&stats_lock);
    overruns++;
    portEXIT_CRITICAL_ISR(&stats_lock);
    return false;
}

static esp_err_t open_audio(void)
{
    if (CONFIG_KVM_AUDIO_SLOT_BITS != 16 && CONFIG_KVM_AUDIO_SLOT_BITS != 32) return ESP_ERR_INVALID_ARG;
    if (CONFIG_KVM_AUDIO_RATE != 32000 && CONFIG_KVM_AUDIO_RATE != 44100 &&
        CONFIG_KVM_AUDIO_RATE != 48000 && CONFIG_KVM_AUDIO_RATE != 88200 &&
        CONFIG_KVM_AUDIO_RATE != 96000) return ESP_ERR_INVALID_ARG;
    if (!GPIO_IS_VALID_GPIO(CONFIG_KVM_AUDIO_BCLK) || !GPIO_IS_VALID_GPIO(CONFIG_KVM_AUDIO_WS) ||
        !GPIO_IS_VALID_GPIO(CONFIG_KVM_AUDIO_DIN) || CONFIG_KVM_AUDIO_BCLK == CONFIG_KVM_AUDIO_WS ||
        CONFIG_KVM_AUDIO_DIN == CONFIG_KVM_AUDIO_WS || CONFIG_KVM_AUDIO_DIN == CONFIG_KVM_AUDIO_BCLK)
        return ESP_ERR_INVALID_ARG;
    // Slave signals are inputs. Clear any output state left by earlier pin users.
    gpio_config_t inputs = {
        .pin_bit_mask = (1ULL << CONFIG_KVM_AUDIO_BCLK) | (1ULL << CONFIG_KVM_AUDIO_WS) |
                        (1ULL << CONFIG_KVM_AUDIO_DIN),
        .mode = GPIO_MODE_INPUT,
    };
    esp_err_t err = gpio_config(&inputs);
    if (err != ESP_OK) return err;
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_SLAVE);
    channel.dma_desc_num = 8;
    channel.dma_frame_num = CONFIG_KVM_AUDIO_RATE / 200;
    err = i2s_new_channel(&channel, NULL, &rx);
    if (err != ESP_OK) return err;
    i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_KVM_AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(CONFIG_KVM_AUDIO_SLOT_BITS, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = CONFIG_KVM_AUDIO_BCLK,
                     .ws = CONFIG_KVM_AUDIO_WS, .dout = I2S_GPIO_UNUSED, .din = CONFIG_KVM_AUDIO_DIN},
    };
    err = i2s_channel_init_std_mode(rx, &config);
    if (err == ESP_OK) {
        i2s_event_callbacks_t callbacks = {.on_recv_q_ovf = overflow};
        err = i2s_channel_register_event_callback(rx, &callbacks, NULL);
    }
    if (err != ESP_OK) { i2s_del_channel(rx); rx = NULL; }
    return err;
}

static void stream_task(void *arg)
{
    httpd_req_t *req = arg;
    // These CPU-side copies need no DMA RAM. At 48 kHz each 5 ms PCM block fits
    // a single TCP packet, reducing batching delay without extra data packets.
    audio_buffers_t *buffers = heap_caps_malloc(sizeof(*buffers), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int16_t *pcm = buffers ? buffers->pcm : NULL;
    int fd = httpd_req_to_sockfd(req);
    int nodelay = 1;
    struct timeval timeout = {.tv_sec = 0, .tv_usec = 100000};
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    char headers[256];
    int header_size = snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n"
        "X-Audio-Rate: %d\r\nX-Audio-Format: s16le-stereo\r\n\r\n", CONFIG_KVM_AUDIO_RATE);
    bool sent = false;
    uint32_t handled_overruns = 0;
    bool discard_backlog = false;
    log_audio_heap("start");
    // Start only once the worker can drain DMA; request/task setup can exceed its budget.
    esp_err_t enable_err = buffers ? i2s_channel_enable(rx) : ESP_ERR_NO_MEM;
    bool running = enable_err == ESP_OK;
    for (;;) {
        if (enable_err != ESP_OK) break;
        if (!kvm_cap_active(KVM_CAP_AUDIO) || !kvm_auth_session_ok(&stream_session)) break;
        size_t received = 0;
        const size_t bytes = AUDIO_SAMPLES * (CONFIG_KVM_AUDIO_SLOT_BITS / 8);
        esp_err_t err = i2s_channel_read(rx, &buffers->raw, bytes, &received, 500);
        if (err != ESP_OK || received == 0) {
            ESP_LOGW("hdmi_audio", "I2S read %s, received=%u requested=%u",
                     esp_err_to_name(err), (unsigned)received, (unsigned)bytes);
            break;
        }
        size_t samples = received / (CONFIG_KVM_AUDIO_SLOT_BITS / 8);
        int32_t maximum = 0;
        uint64_t energy = 0;
        for (size_t i = 0; i < samples; i++) {
            pcm[i] = CONFIG_KVM_AUDIO_SLOT_BITS == 32 ? (int16_t)(buffers->raw.words[i] >> 16) : buffers->raw.halfwords[i];
            int32_t sample = pcm[i];
            int32_t magnitude = abs(sample);
            if (magnitude > maximum) maximum = magnitude;
            energy += (int64_t)sample * sample;
        }
        uint32_t active = 0;
        for (size_t i = 0; i + 1 < samples; i += 2) {
            if (abs(pcm[i]) > 32 || abs(pcm[i + 1]) > 32) active++;
        }
        // Integer accumulation avoids per-sample software double arithmetic.
        float top = maximum / 32768.0f;
        float level = sqrtf((float)energy / samples) / 32768.0f;
        portENTER_CRITICAL(&stats_lock);
        frames += samples / 2;
        active_frames += active;
        if (top > peak_max) peak_max = top;
        peak = top;
        rms = level;
        last_samples = esp_timer_get_time();
        uint32_t lost = overruns;
        portEXIT_CRITICAL(&stats_lock);
        // Video startup/network bursts can briefly delay this worker. Reset the
        // DMA cursor and queue so recovery resumes at live sound, not a backlog.
        if (discard_backlog || lost != handled_overruns) {
            if (i2s_channel_disable(rx) != ESP_OK) break;
            running = false;
            portENTER_CRITICAL(&stats_lock);
            handled_overruns = overruns;
            portEXIT_CRITICAL(&stats_lock);
            enable_err = i2s_channel_enable(rx);
            running = enable_err == ESP_OK;
            discard_backlog = false;
            continue;
        }
        int64_t start = esp_timer_get_time();
        if (!sent) {
            // Close-delimited PCM needs no tiny chunk-framing TCP packets. Once
            // headers start, even a partial send must never fall back to a 503.
            sent = true;
            if (!send_all(req, headers, header_size)) break;
        }
        if (!send_all(req, (const char *)pcm, samples * sizeof(int16_t))) break;
        // A completed slow send is recoverable; the browser drops stale bursts.
        // A send error still closes the connection, including socket timeouts.
        discard_backlog = esp_timer_get_time() - start > 100000;
        // Logout revokes access to the stream as well as ordinary requests.
        if (!kvm_cap_active(KVM_CAP_AUDIO) || !kvm_auth_session_ok(&stream_session)) { break; }
    }
    if (!sent) {
        // Presence check only: polling aliases fast clocks, so this is not a frequency measurement.
        gpio_dump_io_configuration(stdout, (1ULL << CONFIG_KVM_AUDIO_BCLK) |
                                   (1ULL << CONFIG_KVM_AUDIO_WS) | (1ULL << CONFIG_KVM_AUDIO_DIN));
        int b = gpio_get_level(CONFIG_KVM_AUDIO_BCLK), w = gpio_get_level(CONFIG_KVM_AUDIO_WS);
        unsigned bc = 0, wc = 0;
        int64_t until = esp_timer_get_time() + 2000;
        while (esp_timer_get_time() < until) {
            int nb = gpio_get_level(CONFIG_KVM_AUDIO_BCLK), nw = gpio_get_level(CONFIG_KVM_AUDIO_WS);
            bc += nb != b; wc += nw != w; b = nb; w = nw;
        }
        ESP_LOGW("hdmi_audio", "no samples: BCLK changes=%u WS changes=%u in 2ms; levels B=%d W=%d D=%d",
                 bc, wc, b, w, gpio_get_level(CONFIG_KVM_AUDIO_DIN));
    }
    if (running) i2s_channel_disable(rx);
    i2s_del_channel(rx);
    rx = NULL;
    heap_caps_free(buffers);
    ESP_LOGI("hdmi_audio", "stack free minimum=%u", (unsigned)uxTaskGetStackHighWaterMark(NULL));
    log_audio_heap("stop");
    if (!sent) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_sendstr(req, "No usable I2S samples; check BCLK, WFS, SD, ground, source PCM format and slot width");
    }
    // Closing the session terminates the close-delimited response for fetch.
    httpd_handle_t server = req->handle;
    httpd_req_async_handler_complete(req);
    httpd_sess_trigger_close(server, fd);
    stream_session = (kvm_auth_session_ref_t){0};
    atomic_store(&busy, false);
    vTaskDelete(NULL);
}
#endif

void kvm_audio_probe(void)
{
#if CONFIG_KVM_ENABLE_AUDIO
    esp_err_t err = open_audio();
    if (err == ESP_OK) {
        i2s_del_channel(rx);
        rx = NULL;
    }
    // This probes the receiver only. Clock/data presence is established by samples.
    kvm_cap_report(KVM_CAP_AUDIO, err == ESP_OK, "I2S receiver: %s", esp_err_to_name(err));
#else
    kvm_cap_report(KVM_CAP_AUDIO, false, "HDMI audio is not compiled; requires separate I2S wiring");
#endif
}

static esp_err_t audio_status(httpd_req_t *req)
{
    if (!kvm_auth_check(req)) return kvm_auth_challenge(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
#if CONFIG_KVM_ENABLE_AUDIO
    char body[384];
    portENTER_CRITICAL(&stats_lock);
    uint32_t f = frames, o = overruns, a = active_frames;
    float p = peak, r = rms, m = peak_max;
    int64_t age = last_samples ? (esp_timer_get_time() - last_samples) / 1000 : -1;
    portEXIT_CRITICAL(&stats_lock);
    snprintf(body, sizeof(body),
        "{\"available\":%s,\"enabled\":%s,\"streaming\":%s,\"rate\":%d,\"channels\":2,\"frames\":%lu,"
        "\"overruns\":%lu,\"peak\":%.6f,\"rms\":%.6f,\"sample_age_ms\":%lld,"
        "\"active_frames\":%lu,\"peak_max\":%.6f}",
        kvm_cap_available(KVM_CAP_AUDIO) ? "true" : "false",
        kvm_cap_active(KVM_CAP_AUDIO) ? "true" : "false",
        atomic_load(&busy) ? "true" : "false", CONFIG_KVM_AUDIO_RATE,
        (unsigned long)f, (unsigned long)o, (double)p, (double)r, (long long)age,
        (unsigned long)a, (double)m);
    return httpd_resp_sendstr(req, body);
#else
    return httpd_resp_sendstr(req, "{\"available\":false,\"reason\":\"HDMI audio requires I2S wiring and an audio-enabled firmware build\"}");
#endif
}

static esp_err_t audio_stream(httpd_req_t *req)
{
    kvm_auth_session_ref_t session;
    if (!kvm_auth_capture_session(req, &session)) return kvm_auth_challenge(req);
#if CONFIG_KVM_ENABLE_AUDIO
    if (!kvm_cap_active(KVM_CAP_AUDIO)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Enable Stream HDMI audio in Settings; verify I2S wiring and firmware configuration");
    }
    if (atomic_exchange(&busy, true)) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "Audio already has a listener");
    }
    esp_err_t err = open_audio();
    if (err != ESP_OK) {
        atomic_store(&busy, false);
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, esp_err_to_name(err));
    }
    portENTER_CRITICAL(&stats_lock);
    frames = overruns = active_frames = 0; peak = rms = peak_max = 0; last_samples = 0;
    portEXIT_CRITICAL(&stats_lock);
    httpd_req_t *async = NULL;
    err = httpd_req_async_handler_begin(req, &async);
    if (err == ESP_OK) {
        stream_session = session;
        if (xTaskCreate(stream_task, "hdmi_audio", AUDIO_STACK_BYTES, async, 6, NULL) == pdPASS) return ESP_OK;
        stream_session = (kvm_auth_session_ref_t){0};
        httpd_resp_set_status(async, "503 Service Unavailable");
        httpd_resp_sendstr(async, "Audio task unavailable");
        httpd_req_async_handler_complete(async);
    }
    i2s_del_channel(rx); rx = NULL;
    atomic_store(&busy, false);
    return err == ESP_OK ? ESP_OK : ESP_FAIL;
#else
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_sendstr(req, "HDMI audio is not configured");
#endif
}

const httpd_uri_t *audio_api_routes(size_t *count)
{
    static const httpd_uri_t routes[] = {
        {.uri = "/api/v1/audio/status", .method = HTTP_GET, .handler = audio_status},
        {.uri = "/audio.pcm", .method = HTTP_GET, .handler = audio_stream},
    };
    *count = sizeof(routes) / sizeof(routes[0]);
    return routes;
}
