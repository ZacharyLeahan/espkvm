/* SPDX-License-Identifier: Apache-2.0 */
#include "sdkconfig.h"
#include "lcd_record_control.h"
#include "capture_lcd_control.h"
#include "capture.h"
#include "video_frame.h"
#include "kvm_record.h"
#include "kvm_storage.h"
#include "kvm_settings.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#if CONFIG_KVM_DSI_STATUS_STRIP
static portMUX_TYPE s_mu = portMUX_INITIALIZER_UNLOCKED;
static capture_lcd_record_state_t s_state;
static TaskHandle_t s_task;
static bool s_owned;
static int32_t s_original[3];
/* Configure rate before opening H.264 so its SPS matches the requested cap. */
static const char *s_keys[] = {"vid_fps_max", "h264_kbps", "vid_codec"};
static const int32_t s_trial[] = {30, 4000, 1};

static void get_state(capture_lcd_record_state_t *out)
{
    taskENTER_CRITICAL(&s_mu);
    *out = s_state;
    taskEXIT_CRITICAL(&s_mu);
}

static void toggle(void)
{
    taskENTER_CRITICAL(&s_mu);
    bool accept = !s_state.busy;
    if (accept) s_state.busy = true;
    taskEXIT_CRITICAL(&s_mu);
    if (accept) xTaskNotifyGive(s_task);
}

static void restore_settings(void)
{
    /* Do not overwrite a deliberate setting change made in the web UI. */
    for (int i = 2; i >= 0; --i) {
        if (kvm_setting_int(s_keys[i]) == s_trial[i])
            (void)kvm_setting_set_int(s_keys[i], s_original[i]);
    }
    s_owned = false;
}

static void worker(void *arg)
{
    (void)arg;
    char message[32] = "";
    for (;;) {
        bool pressed = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) != 0;
        kvm_record_status_t rec;
        kvm_record_status(&rec);
        if (s_owned && !rec.recording) {
            ESP_LOGI("lcd_record", "recording ended: %s", rec.stopped);
            snprintf(message, sizeof(message), "RECORDING ENDED");
            restore_settings();
        }
        if (pressed) {
            message[0] = 0;
            if (rec.recording) {
                kvm_record_stop("stopped from LCD");
                if (s_owned) restore_settings();
            } else if (rec.dashcam) {
                snprintf(message, sizeof(message), "DASHCAM ACTIVE");
            } else if (!kvm_storage_writable()) {
                snprintf(message, sizeof(message), "SD NOT WRITABLE");
            } else {
                kvm_video_status_t video;
                capture_status_get(&video);
                if (!video.signal || video.too_fast) {
                    snprintf(message, sizeof(message), "NO HDMI SIGNAL");
                } else {
                    for (unsigned i = 0; i < 3; ++i) s_original[i] = kvm_setting_int(s_keys[i]);
                    s_owned = true;
                    esp_err_t err = ESP_OK;
                    for (unsigned i = 0; i < 3 && err == ESP_OK; ++i)
                        err = kvm_setting_set_int(s_keys[i], s_trial[i]);
                    /* Let the capture task park the old codec before borrowing
                     * its arena. Never block the CSI or touch task on SD I/O. */
                    for (unsigned attempt = 0; err == ESP_OK && attempt < 60 &&
                         video_frame_payload() != VIDEO_PAYLOAD_H264; ++attempt)
                        vTaskDelay(pdMS_TO_TICKS(50));
                    char why[128] = "settings failed";
                    if (err == ESP_OK) err = kvm_record_start(0, why, sizeof(why));
                    if (err != ESP_OK) {
                        ESP_LOGW("lcd_record", "start failed: %s", why);
                        snprintf(message, sizeof(message), "REC FAILED: SEE LOG");
                        restore_settings();
                    } else ESP_LOGI("lcd_record", "recording started from touch");
                }
            }
            kvm_record_status(&rec);
        }
        taskENTER_CRITICAL(&s_mu);
        s_state.available = true;
        s_state.recording = rec.recording;
        s_state.local_consumer = rec.recording || rec.dashcam;
        s_state.seconds = rec.seconds;
        if (pressed) s_state.busy = false;
        snprintf(s_state.message, sizeof(s_state.message), "%s", message);
        taskEXIT_CRITICAL(&s_mu);
    }
}
#endif

void lcd_record_control_init(void)
{
#if CONFIG_KVM_DSI_STATUS_STRIP
    if (xTaskCreate(worker, "lcd_record", 6144, NULL, 3, &s_task) == pdPASS)
        capture_lcd_control_register(get_state, toggle);
#endif
}
