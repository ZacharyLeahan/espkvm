// SPDX-License-Identifier: Apache-2.0
#include "sdkconfig.h"
#include "web_priv.h"
#include "kvm_auth.h"
#include "kvm_audio.h"
#include "kvm_caps.h"
#include "http_server.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#if CONFIG_KVM_ENABLE_AUDIO
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <inttypes.h>
#include "lwip/sockets.h"
#include "lwip/tcp.h"

static atomic_bool busy;
typedef struct {
    httpd_req_t *req;
    kvm_audio_subscription_t *sub;
    kvm_auth_session_ref_t session;
    int64_t started_us;
} stream_context_t;

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

static void stream_task(void *arg)
{
    stream_context_t *ctx = arg;
    httpd_req_t *req = ctx->req;
    kvm_audio_block_t *block = heap_caps_malloc(sizeof(*block), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int fd = httpd_req_to_sockfd(req);
    int nodelay = 1;
    struct timeval timeout = {.tv_sec = 0, .tv_usec = 100000};
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    char headers[256];
    int header_size = snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n"
        "X-Audio-Rate: %lu\r\nX-Audio-Format: s16le-stereo\r\n\r\n", (unsigned long)kvm_audio_rate());
    bool sent = false;
    uint64_t sent_frames = 0, gap_frames = 0, next_frame = 0;
    uint32_t epoch = 0, resets = 0, slow_sends = 0;
    bool have_frame = false;
    int64_t first_us = 0, max_send_us = 0, max_age_us = 0, max_ok_send_us = 0;
    int64_t report_us = ctx->started_us;
    const char *ended = "disabled_or_session";
    while (block && kvm_cap_active(KVM_CAP_AUDIO) && kvm_auth_session_ok(&ctx->session)) {
        esp_err_t error = kvm_audio_read(ctx->sub, block, 600);
        if (error != ESP_OK) { ended = "capture_read"; break; }
        // Recheck after waiting, before sending even the first PCM block.
        if (!kvm_cap_active(KVM_CAP_AUDIO) || !kvm_auth_session_ok(&ctx->session)) break;
        const int64_t now = esp_timer_get_time();
        if (!first_us) first_us = now;
        const int64_t age = now - block->start_us;
        if (age > max_age_us) max_age_us = age;
        if (!sent) {
            // Once a custom header starts, never append a second HTTP response.
            sent = true;
            if (!send_all(req, headers, header_size)) { ended = "header_send"; break; }
        }
        const int64_t sending = esp_timer_get_time();
        const bool ok = send_all(req, (const char *)block->pcm,
                                 block->frames * KVM_AUDIO_CHANNELS * sizeof(int16_t));
        const int64_t send_us = esp_timer_get_time() - sending;
        if (send_us > max_send_us) max_send_us = send_us;
        if (send_us > 5000) slow_sends++;
        if (!ok) { ended = "pcm_send"; break; }
        if (send_us > max_ok_send_us) max_ok_send_us = send_us;
        if (have_frame) {
            if (epoch != block->epoch) resets++;
            else if (block->frame_index > next_frame) gap_frames += block->frame_index - next_frame;
        }
        have_frame = true;
        epoch = block->epoch;
        next_frame = block->frame_index + block->frames;
        sent_frames += block->frames;
        if (esp_timer_get_time() - report_us >= 30000000) {
            ESP_LOGI("audio_stream", "progress sent=%" PRIu64 " gaps=%" PRIu64
                     " max_ok_send_us=%" PRId64, sent_frames, gap_frames, max_ok_send_us);
            report_us = esp_timer_get_time();
        }
        // Backpressure affects only this subscriber's bounded queue. The shared
        // I2S producer and SD recorder continue even if this socket times out.
    }
    kvm_audio_subscription_stats_t stats;
    kvm_audio_subscription_stats(ctx->sub, &stats);
    // Keep each line within the retained log's 160-byte limit. Never log PCM,
    // cookies or source text; these counters describe transport only.
    ESP_LOGI("audio_stream", "end=%s sent=%" PRIu64 " read=%" PRIu64,
             ended, sent_frames, stats.received_frames);
    ESP_LOGI("audio_stream", "queue_drop=%" PRIu64 " gaps=%" PRIu64 " epochs=%" PRIu32,
             stats.dropped_frames, gap_frames, resets);
    ESP_LOGI("audio_stream", "startup_us=%" PRId64 " max_send_us=%" PRId64
             " slow_sends=%" PRIu32 " max_age_us=%" PRId64,
             first_us ? first_us - ctx->started_us : -1,
             max_send_us, slow_sends, max_age_us);
    ESP_LOGI("audio_stream", "max_ok_send_us=%" PRId64, max_ok_send_us);
    kvm_audio_unsubscribe(ctx->sub);
    heap_caps_free(block);
    if (!sent) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_sendstr(req, "No usable I2S samples; check BCLK, WFS, SD, ground, source PCM format and slot width");
    }
    httpd_handle_t server = req->handle;
    httpd_req_async_handler_complete(req);
    httpd_sess_trigger_close(server, fd);
    free(ctx);
    atomic_store(&busy, false);
    vTaskDelete(NULL);
}
#endif

void kvm_audio_probe(void)
{
#if CONFIG_KVM_ENABLE_AUDIO
    esp_err_t err = kvm_audio_init();
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
    kvm_audio_status_t audio;
    kvm_audio_get_status(&audio);
    int64_t age = audio.last_samples_us ? (esp_timer_get_time() - audio.last_samples_us) / 1000 : -1;
    char body[448];
    snprintf(body, sizeof(body),
        "{\"available\":%s,\"enabled\":%s,\"streaming\":%s,\"rate\":%lu,\"channels\":2,\"frames\":%llu,"
        "\"overruns\":%lu,\"peak\":%.6f,\"rms\":%.6f,\"sample_age_ms\":%lld,"
        "\"active_frames\":%llu,\"peak_max\":%.6f}",
        kvm_cap_available(KVM_CAP_AUDIO) ? "true" : "false",
        kvm_cap_active(KVM_CAP_AUDIO) ? "true" : "false",
        atomic_load(&busy) ? "true" : "false", (unsigned long)audio.rate,
        (unsigned long long)audio.frames, (unsigned long)audio.overruns,
        (double)audio.peak, (double)audio.rms, (long long)age,
        (unsigned long long)audio.active_frames, (double)audio.peak_max);
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
        return httpd_resp_sendstr(req, "Enable HDMI audio in Settings; verify I2S wiring and firmware configuration");
    }
    if (atomic_exchange(&busy, true)) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "Audio already has a browser listener");
    }
    stream_context_t *ctx = calloc(1, sizeof(*ctx));
    esp_err_t err = ctx ? kvm_audio_subscribe(KVM_AUDIO_BROWSER, &ctx->sub) : ESP_ERR_NO_MEM;
    if (err != ESP_OK) {
        free(ctx);
        atomic_store(&busy, false);
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, esp_err_to_name(err));
    }
    ctx->session = session;
    ctx->started_us = esp_timer_get_time();
    err = httpd_req_async_handler_begin(req, &ctx->req);
    if (err == ESP_OK) {
        if (xTaskCreate(stream_task, "hdmi_audio", 6144, ctx, 6, NULL) == pdPASS) return ESP_OK;
        httpd_resp_set_status(ctx->req, "503 Service Unavailable");
        httpd_resp_sendstr(ctx->req, "Audio task unavailable");
        httpd_req_async_handler_complete(ctx->req);
    }
    kvm_audio_unsubscribe(ctx->sub);
    free(ctx);
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
