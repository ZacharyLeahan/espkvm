/*
 * SPDX-FileCopyrightText: 2026 ESP-KVM contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Experimental, local-only preview for the Waveshare 3.5inch DSI LCD (E).
 * The panel's Raspberry Pi overlay describes a generic one-lane RGB888 DSI
 * video sink at 640x480 with a 24 MHz pixel clock. Its timing is not the HDMI
 * timing: every captured mode is fitted into this fixed 640x480 framebuffer.
 *
 * The CSI task calls this only while it holds a completed source buffer. A
 * five-fps limit keeps the CPU scaler from taking the time the web encoder
 * needs; neither its codec selection nor viewer accounting is changed.
 */
#include "capture_priv.h"
#include "capture.h"

#include <string.h>

#include "esp_cache.h"
#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "hal/mipi_dsi_host_ll.h"
#include "hal/mipi_dsi_brg_ll.h"
#include "esp_task_wdt.h"
#include "soc/mipi_dsi_host_struct.h"
#include "soc/mipi_dsi_bridge_struct.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_KVM_DSI_PREVIEW

#define TAG "dsi_preview"
#define LCD_W 640u
#define LCD_H 480u
#define LCD_FB_BYTES (LCD_W * LCD_H * 3u)
#define PREVIEW_INTERVAL_US 200000
#define ICN_READ_TIMEOUT_US 100000

static esp_ldo_channel_handle_t s_phy_power;
static i2c_master_dev_handle_t s_ws_control;
static esp_lcd_dsi_bus_handle_t s_bus;
static esp_lcd_panel_io_handle_t s_dbi_io;
static esp_lcd_panel_handle_t s_panel;
static uint8_t *s_fb;
static int64_t s_last_frame_us;
static uint32_t s_last_w;
static uint32_t s_last_h;
static uint32_t s_frames;
static int64_t s_report_us;
static bool s_link_logged;
static volatile uint32_t s_dma_frames;

static bool on_dsi_frame_complete(esp_lcd_panel_handle_t panel,
                                  esp_lcd_dpi_panel_event_data_t *event_data, void *user_ctx)
{
    (void)panel;
    (void)event_data;
    (void)user_ctx;
    ++s_dma_frames;
    return false;
}

static void log_dsi_link(const char *phase)
{
    ESP_LOGI(TAG, "DSI %s: phy=0x%08lx mode=0x%08lx cfg=0x%08lx active=0x%08lx err0=0x%08lx err1=0x%08lx",
             phase,
             (unsigned long)MIPI_DSI_HOST.phy_status.val,
             (unsigned long)MIPI_DSI_HOST.mode_cfg.val,
             (unsigned long)MIPI_DSI_HOST.vid_mode_cfg.val,
             (unsigned long)MIPI_DSI_HOST.vid_mode_cfg_act.val,
             (unsigned long)MIPI_DSI_HOST.int_st0.val,
             (unsigned long)MIPI_DSI_HOST.int_st1.val);
    ESP_LOGI(TAG, "DSI bridge %s: en=0x%08lx misc=0x%08lx fifo=%lu raw=0x%08lx int=0x%08lx dma_frames=%lu fb=%p",
             phase,
             (unsigned long)MIPI_DSI_BRIDGE.en.val,
             (unsigned long)MIPI_DSI_BRIDGE.dpi_misc_config.val,
             (unsigned long)MIPI_DSI_BRIDGE.fifo_flow_status.raw_buf_depth,
             (unsigned long)MIPI_DSI_BRIDGE.int_raw.val,
             (unsigned long)MIPI_DSI_BRIDGE.int_st.val,
             (unsigned long)s_dma_frames, s_fb);
}

static void preview_cleanup(void)
{
    if (s_panel) {
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
    }
    if (s_dbi_io) {
        esp_lcd_panel_io_del(s_dbi_io);
        s_dbi_io = NULL;
    }
    if (s_bus) {
        esp_lcd_del_dsi_bus(s_bus);
        s_bus = NULL;
    }
    if (s_phy_power) {
        esp_ldo_release_channel(s_phy_power);
        s_phy_power = NULL;
    }
    if (s_ws_control) {
        i2c_master_bus_rm_device(s_ws_control);
        s_ws_control = NULL;
    }
    s_fb = NULL;
}

static void waveshare_control_write(uint8_t reg, uint8_t value)
{
    if (!s_ws_control) {
        return;
    }
    const uint8_t command[2] = {reg, value};
    esp_err_t err = i2c_master_transmit(s_ws_control, command, sizeof(command), 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Waveshare control register 0x%02x write failed: %s",
                 reg, esp_err_to_name(err));
    }
}

static void waveshare_control_init(void)
{
    i2c_master_bus_handle_t bus = capture_i2c_bus();
    if (!bus) {
        ESP_LOGW(TAG, "display control probe skipped: shared I2C bus unavailable");
        return;
    }
    const bool touch14 = i2c_master_probe(bus, 0x14, 100) == ESP_OK;
    const bool touch5d = i2c_master_probe(bus, 0x5d, 100) == ESP_OK;
    const bool bridge45 = i2c_master_probe(bus, 0x45, 100) == ESP_OK;
    /* ICN6211's two selectable 7-bit I2C addresses are 0x2c/0x2d. This is
     * an ACK-only probe; its onboard MCU may own a separate I2C bus. */
    const bool icn2c = i2c_master_probe(bus, 0x2c, 100) == ESP_OK;
    const bool icn2d = i2c_master_probe(bus, 0x2d, 100) == ESP_OK;
    ESP_LOGI(TAG, "display I2C probe: touch 0x14=%d 0x5d=%d control 0x45=%d ICN6211 0x2c=%d 0x2d=%d",
             touch14, touch5d, bridge45, icn2c, icn2d);
    if (!bridge45) {
        return;
    }
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x45,
        .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &config, &s_ws_control);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Waveshare display control setup failed: %s", esp_err_to_name(err));
        return;
    }
    /* Mainline Linux's Waveshare DSI2DPI bridge driver initializes the
     * controller through these three registers before enabling DSI video. */
    waveshare_control_write(0xc0, 0x01);
    waveshare_control_write(0xc2, 0x01);
    waveshare_control_write(0xac, 0x01);
    waveshare_control_write(0xab, 0x00);
    waveshare_control_write(0xaa, 0x01);
    waveshare_control_write(0xad, 0x01);
}

static void fill_startup_pattern(void)
{
    /* A checkerboard is deliberately unlike the ICN6211's internal color-bar
     * test pattern. It distinguishes a working DSI video link from the panel
     * merely powering up with no usable signal. */
    for (uint32_t y = 0; y < LCD_H; ++y) {
        for (uint32_t x = 0; x < LCD_W; ++x) {
            uint8_t *p = s_fb + ((size_t)y * LCD_W + x) * 3u;
            bool yellow = ((x / 80u) + (y / 60u)) & 1u;
            p[0] = yellow ? 255 : 0;
            p[1] = yellow ? 255 : 0;
            p[2] = 0;
        }
    }
    (void)esp_cache_msync(s_fb, LCD_FB_BYTES,
                          ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}

#if CONFIG_KVM_DSI_PREVIEW_ICN6211_PROBE || CONFIG_KVM_DSI_PREVIEW_ICN6211_DISABLE_BIST
static esp_err_t icn6211_read_register(uint8_t reg, uint8_t *value)
{
    dsi_host_dev_t *host = &MIPI_DSI_HOST;
    const int64_t deadline = esp_timer_get_time() + ICN_READ_TIMEOUT_US;

    /* Linux's ICN6211 driver issues a two-byte generic read request:
     * register address followed by requested response length. Do not use
     * IDF's read helper here: it waits forever for an absent reply. */
    while (mipi_dsi_host_ll_gen_is_cmd_fifo_full(host)) {
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }
    mipi_dsi_host_ll_gen_set_packet_header(host, 0, MIPI_DSI_DT_SET_MAXIMUM_RETURN_PKT, 0, 1);
    while (!mipi_dsi_host_ll_gen_is_cmd_fifo_empty(host)) {
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }

    mipi_dsi_host_ll_enable_bta(host, true);
    mipi_dsi_host_ll_gen_set_rx_vcid(host, 0);
    mipi_dsi_host_ll_gen_set_packet_header(host, 0, MIPI_DSI_DT_GENERIC_READ_REQUEST_2, 1, reg);
    while (mipi_dsi_host_ll_gen_is_read_cmd_busy(host)) {
        if (esp_timer_get_time() >= deadline) {
            mipi_dsi_host_ll_enable_bta(host, false);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }
    while (mipi_dsi_host_ll_gen_is_read_fifo_empty(host)) {
        if (esp_timer_get_time() >= deadline) {
            mipi_dsi_host_ll_enable_bta(host, false);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }
    *value = (uint8_t)mipi_dsi_host_ll_gen_read_payload_fifo(host);
    mipi_dsi_host_ll_enable_bta(host, false);
    return ESP_OK;
}
#endif

#if CONFIG_KVM_DSI_PREVIEW_ICN6211_PROBE
static void icn6211_probe(void)
{
    /* IDs first; then inspect the bridge's current BIST, geometry, clock,
     * lane, and configuration state without overriding its Nuvoton MCU. */
    static const uint8_t regs[] = {
        0x00, 0x01, 0x02, 0x03, 0x09, 0x10, 0x11, 0x1e,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x27, 0x28,
        0x29, 0x2a, 0x56, 0x69, 0x6b, 0x7a, 0x86, 0xb5, 0xb6,
    };
    for (size_t i = 0; i < sizeof(regs); ++i) {
        const uint8_t reg = regs[i];
        uint8_t value = 0;
        esp_err_t err = icn6211_read_register(reg, &value);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ICN6211 register 0x%02x: %s; stopping probe",
                     reg, esp_err_to_name(err));
            break;
        }
        ESP_LOGI(TAG, "ICN6211 register 0x%02x = 0x%02x", reg, value);
    }
    log_dsi_link("after ICN6211 read-only probe");
}
#endif

#if CONFIG_KVM_DSI_PREVIEW_ICN6211_DISABLE_BIST
static esp_err_t icn6211_write_register(uint8_t reg, uint8_t value)
{
    dsi_host_dev_t *host = &MIPI_DSI_HOST;
    const int64_t deadline = esp_timer_get_time() + ICN_READ_TIMEOUT_US;
    while (mipi_dsi_host_ll_gen_is_cmd_fifo_full(host)) {
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }
    /* Linux's ICN6211 DSI regmap sends {register, value} as a two-byte
     * generic short write; this is not a DCS display command. */
    mipi_dsi_host_ll_gen_set_packet_header(host, 0, MIPI_DSI_DT_GENERIC_SHORT_WRITE_2,
                                           value, reg);
    while (!mipi_dsi_host_ll_gen_is_cmd_fifo_empty(host)) {
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }
    return ESP_OK;
}

static esp_err_t icn6211_disable_bist(void)
{
    const uint8_t expected_id[] = {0xc1, 0x62, 0x11};
    for (uint8_t reg = 0; reg < sizeof(expected_id); ++reg) {
        uint8_t value;
        ESP_RETURN_ON_ERROR(icn6211_read_register(reg, &value), TAG, "read ICN6211 ID failed");
        if (value != expected_id[reg]) {
            ESP_LOGE(TAG, "unexpected ICN6211 ID byte %u: 0x%02x", reg, value);
            return ESP_ERR_INVALID_RESPONSE;
        }
    }
    uint8_t before;
    ESP_RETURN_ON_ERROR(icn6211_read_register(0x2a, &before), TAG, "read BIST failed");
    const uint8_t after = before & ~0x08u; /* BIST_POL_BIST_GEN */
    ESP_LOGI(TAG, "ICN6211 BIST 0x2a: 0x%02x -> 0x%02x", before, after);
    if (before == after) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(icn6211_write_register(0x2a, after), TAG, "write BIST failed");
    uint8_t verify;
    ESP_RETURN_ON_ERROR(icn6211_read_register(0x2a, &verify), TAG, "verify BIST failed");
    if (verify != after) {
        ESP_LOGE(TAG, "ICN6211 BIST write did not stick: 0x%02x", verify);
        return ESP_ERR_INVALID_RESPONSE;
    }
    ESP_LOGI(TAG, "ICN6211 BIST enable bit confirmed clear; starting checkerboard");
    return ESP_OK;
}

static void icn6211_inspect_video(void)
{
    /* Generic read responses require command mode on this host. Stop DPI
     * first so its DMA source does not continue feeding a stopped video sink. */
    /* In a live build this runs on the watched camera task. Keep the same
     * settling interval without mistaking the intentional sleep for a hang.
     * Standalone startup runs on a task that may not be subscribed. */
    for (unsigned i = 0; i < 10; ++i) {
        if (esp_task_wdt_status(NULL) == ESP_OK) {
            esp_task_wdt_reset();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (esp_task_wdt_status(NULL) == ESP_OK) {
        esp_task_wdt_reset();
    }
    log_dsi_link("before paused bridge read");
    mipi_dsi_brg_ll_enable_dpi_output(&MIPI_DSI_BRIDGE, false);
    mipi_dsi_brg_ll_update_dpi_config(&MIPI_DSI_BRIDGE);
    vTaskDelay(pdMS_TO_TICKS(20));
    mipi_dsi_host_ll_enable_video_mode(&MIPI_DSI_HOST, false);
    vTaskDelay(pdMS_TO_TICKS(20));
    static const uint8_t regs[] = {0x00, 0x01, 0x02, 0x2a, 0x80, 0x81,
                                   0x84, 0x85, 0x86, 0x87, 0x56, 0x69, 0x6b, 0x7a};
    uint8_t bist = 0;
    for (size_t i = 0; i < sizeof(regs); ++i) {
        uint8_t value;
        esp_err_t err = icn6211_read_register(regs[i], &value);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "paused ICN6211 0x%02x: %s", regs[i], esp_err_to_name(err));
            break;
        }
        ESP_LOGI(TAG, "paused ICN6211 0x%02x = 0x%02x", regs[i], value);
        if (regs[i] == 0x2a) {
            bist = value;
        }
    }
#if CONFIG_KVM_DSI_PREVIEW_ICN6211_MIPI_PLL
    if (bist & 0x08) {
        esp_err_t err = icn6211_write_register(0x7a, 0xc1);
        if (err == ESP_OK) {
            err = icn6211_write_register(0x2a, bist & ~0x08u);
        }
        ESP_LOGI(TAG, "post-video BIST clear: %s", esp_err_to_name(err));
    }
#else
    (void)bist;
#endif
    mipi_dsi_host_ll_enable_video_mode(&MIPI_DSI_HOST, true);
    mipi_dsi_brg_ll_enable_dpi_output(&MIPI_DSI_BRIDGE, true);
    mipi_dsi_brg_ll_update_dpi_config(&MIPI_DSI_BRIDGE);
}

#if CONFIG_KVM_DSI_PREVIEW_ICN6211_MIPI_PLL
static esp_err_t icn6211_configure_mipi_pll(void)
{
    /* ICN6211's configuration tool and Linux driver use 0xc1 to permit DSI
     * writes. At 576 Mb/s the PLL reference is 144 MHz. P=6, M=16, S=16
     * gives the panel's existing 24 MHz pixel clock. Geometry stays intact. */
    static const uint8_t setup[][2] = {
        {0x7a, 0xc1}, {0x6b, 0x73}, {0x69, 0x10},
        {0x56, 0x92}, {0x09, 0x10},
    };
    for (size_t i = 0; i < sizeof(setup) / sizeof(setup[0]); ++i) {
        ESP_RETURN_ON_ERROR(icn6211_write_register(setup[i][0], setup[i][1]),
                            TAG, "MIPI PLL setup write failed");
        vTaskDelay(pdMS_TO_TICKS(10));
        uint8_t actual;
        ESP_RETURN_ON_ERROR(icn6211_read_register(setup[i][0], &actual),
                            TAG, "MIPI PLL setup readback failed");
        ESP_LOGI(TAG, "ICN6211 PLL register 0x%02x expected=0x%02x actual=0x%02x",
                 setup[i][0], setup[i][1], actual);
        ESP_RETURN_ON_FALSE(actual == setup[i][1], ESP_ERR_INVALID_RESPONSE,
                            TAG, "MIPI PLL setup rejected");
    }
    return ESP_OK;
}
#endif

#endif

void capture_dsi_preview_init(void)
{
    waveshare_control_init();
    const esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = 3, /* Function EV: LDO_VO3 -> VDD_MIPI_DPHY */
        .voltage_mv = 2500,
    };
    esp_err_t err = esp_ldo_acquire_channel(&ldo_cfg, &s_phy_power);
    if (err != ESP_OK) {
        goto fail;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    const esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = 1,
        .lane_bit_rate_mbps = 576, /* non-burst RGB888: 24 MHz * 24 bits */
#if CONFIG_KVM_DSI_PREVIEW_ICN6211_MIPI_PLL
        .flags.clock_lane_force_hs = true,
#endif
    };
    err = esp_lcd_new_dsi_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        goto fail;
    }
    /* Raspberry Pi's VC4 DSI host disables HS end-of-transmission packets for
     * this generic panel; IDF enables them by default. */
    MIPI_DSI_HOST.pckhdl_cfg.eotp_tx_en = 0;

    /* Waveshare's ESP DSI component performs bridge control, waits one second,
     * then sends MADCTL, sleep-out and display-on DCS commands before starting
     * DPI video. The Pi overlay does not describe these commands, so keep this
     * sequence confined to the opt-in experimental preview. */
    vTaskDelay(pdMS_TO_TICKS(1000));
    const esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    err = esp_lcd_new_panel_io_dbi(s_bus, &dbi_cfg, &s_dbi_io);
    if (err != ESP_OK) {
        goto fail;
    }
#if CONFIG_KVM_DSI_PREVIEW_ICN6211_PROBE
    ESP_LOGW(TAG, "read-only ICN6211 DSI probe; no framebuffer video will start");
    icn6211_probe();
    return;
#endif
#if CONFIG_KVM_DSI_PREVIEW_ICN6211_DISABLE_BIST
    err = icn6211_disable_bist();
    if (err != ESP_OK) {
        goto fail;
    }
#if CONFIG_KVM_DSI_PREVIEW_ICN6211_MIPI_PLL
    err = icn6211_configure_mipi_pll();
    if (err != ESP_OK) {
        goto fail;
    }
#endif
#endif
    const uint8_t dcs_zero = 0;
    err = esp_lcd_panel_io_tx_param(s_dbi_io, LCD_CMD_MADCTL, &dcs_zero, 1);
    if (err != ESP_OK) {
        goto fail;
    }
    err = esp_lcd_panel_io_tx_param(s_dbi_io, LCD_CMD_SLPOUT, &dcs_zero, 1);
    if (err != ESP_OK) {
        goto fail;
    }
    vTaskDelay(pdMS_TO_TICKS(120));
    err = esp_lcd_panel_io_tx_param(s_dbi_io, LCD_CMD_DISPON, &dcs_zero, 1);
    if (err != ESP_OK) {
        goto fail;
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    const esp_lcd_dpi_panel_config_t dpi_cfg = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 24,
        .in_color_format = LCD_COLOR_FMT_RGB888,
        .out_color_format = LCD_COLOR_FMT_RGB888,
        .num_fbs = 1,
        .video_timing = {
            .h_size = LCD_W,
            .v_size = LCD_H,
            .hsync_front_porch = 48,
            .hsync_pulse_width = 32,
            .hsync_back_porch = 80,
            .vsync_front_porch = 3,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 13,
        },
    };
    err = esp_lcd_new_panel_dpi(s_bus, &dpi_cfg, &s_panel);
    if (err != ESP_OK) {
        goto fail;
    }
    err = esp_lcd_dpi_panel_get_frame_buffer(s_panel, 1, (void **)&s_fb);
    if (err != ESP_OK || !s_fb) {
        goto fail;
    }
    fill_startup_pattern();
    const esp_lcd_dpi_panel_event_callbacks_t callbacks = {
        .on_frame_buf_complete = on_dsi_frame_complete,
    };
    err = esp_lcd_dpi_panel_register_event_callbacks(s_panel, &callbacks, NULL);
    if (err != ESP_OK) {
        goto fail;
    }
    /* The Waveshare DT overlay requests plain MIPI_DSI_MODE_VIDEO (non-burst).
     * IDF currently hardcodes burst video and per-frame BTA acknowledgements
     * for all DPI panels; this generic bridge has no DSI command response.
     * Set these before panel_init starts video output. */
    MIPI_DSI_HOST.vid_mode_cfg.vid_mode_type = 1; /* non-burst, sync events */
    MIPI_DSI_HOST.vid_mode_cfg.frame_bta_ack_en = 0;
    err = esp_lcd_panel_init(s_panel);
    if (err != ESP_OK) {
        goto fail;
    }
#if CONFIG_KVM_DSI_PREVIEW_HOST_PATTERN
    err = esp_lcd_dpi_panel_set_pattern(s_panel, MIPI_DSI_PATTERN_BAR_VERTICAL);
    if (err != ESP_OK) {
        goto fail;
    }
    ESP_LOGW(TAG, "DSI host vertical-bar diagnostic active; Xbox preview hidden");
#endif
    vTaskDelay(pdMS_TO_TICKS(100));
    log_dsi_link("startup");
    ESP_LOGI(TAG, "Waveshare 3.5 DSI preview started: 640x480 RGB888, 5 updates/s");
#if CONFIG_KVM_DSI_PREVIEW_ICN6211_DISABLE_BIST
    icn6211_inspect_video();
#endif
    return;

fail:
    ESP_LOGE(TAG, "DSI preview unavailable: %s; capture and browser continue",
             esp_err_to_name(err));
    preview_cleanup();
}

bool capture_dsi_preview_due(void)
{
    return s_fb && esp_timer_get_time() - s_last_frame_us >= PREVIEW_INTERVAL_US;
}

static uint8_t clamp8(int v)
{
    return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

static void source_pixel(const uint8_t *src, uint32_t src_w, uint32_t x, uint32_t y,
                         const capture_pixfmt_t *fmt, uint8_t *dst)
{
    const size_t pos = (size_t)y * src_w + x;
    if (fmt->bpp == 24) {
        const uint8_t *p = src + pos * 3u;
        if (strcmp(fmt->name, "bgr888") == 0) {
            dst[0] = p[2]; dst[1] = p[1]; dst[2] = p[0];
        } else {
            dst[0] = p[0]; dst[1] = p[1]; dst[2] = p[2];
        }
        return;
    }

    /* Packed 4:2:2: the TC358743 uses UYVY; the LT6911D uses YUYV. */
    const uint8_t *p = src + (pos & ~(size_t)1u) * 2u;
    const int luma = fmt->luma_first ? p[(x & 1u) ? 2 : 0]
                                       : p[(x & 1u) ? 3 : 1];
    const int u = (fmt->luma_first ? p[1] : p[0]) - 128;
    const int v = (fmt->luma_first ? p[3] : p[2]) - 128;
    const int yy = 298 * (luma - 16);
    dst[0] = clamp8((yy + 409 * v + 128) >> 8);
    dst[1] = clamp8((yy - 100 * u - 208 * v + 128) >> 8);
    dst[2] = clamp8((yy + 516 * u + 128) >> 8);
}

void capture_dsi_preview_frame(const void *src, uint32_t width, uint32_t height,
                               const capture_pixfmt_t *fmt)
{
    if (!capture_dsi_preview_due() || !src || !width || !height || !fmt ||
        (fmt->bpp != 16 && fmt->bpp != 24)) {
        return;
    }
    s_last_frame_us = esp_timer_get_time();

    /* Preserve the source aspect ratio, with black bars rather than stretching
     * the Xbox's 16:9 modes into the panel's 4:3 glass. */
    uint32_t out_w = LCD_W;
    uint32_t out_h = (uint32_t)((uint64_t)height * LCD_W / width);
    if (out_h > LCD_H) {
        out_h = LCD_H;
        out_w = (uint32_t)((uint64_t)width * LCD_H / height);
    }
    if (!out_w || !out_h) {
        return;
    }
    const uint32_t x0 = (LCD_W - out_w) / 2u;
    const uint32_t y0 = (LCD_H - out_h) / 2u;
    if (width != s_last_w || height != s_last_h) {
        memset(s_fb, 0, LCD_FB_BYTES);
        s_last_w = width;
        s_last_h = height;
        ESP_LOGI(TAG, "preview input %lux%lu -> %lux%lu", (unsigned long)width,
                 (unsigned long)height, (unsigned long)out_w, (unsigned long)out_h);
    }
    for (uint32_t dy = 0; dy < out_h; ++dy) {
        const uint32_t sy = (uint32_t)((uint64_t)dy * height / out_h);
        uint8_t *line = s_fb + ((size_t)(dy + y0) * LCD_W + x0) * 3u;
        for (uint32_t dx = 0; dx < out_w; ++dx) {
            const uint32_t sx = (uint32_t)((uint64_t)dx * width / out_w);
            source_pixel(src, width, sx, sy, fmt, line + dx * 3u);
        }
    }
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, LCD_W, LCD_H, s_fb);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "frame submit failed: %s", esp_err_to_name(err));
    }
    s_frames++;
    if (!s_link_logged && s_frames >= 5) {
        s_link_logged = true;
        log_dsi_link("stream");
    }
    const int64_t now = esp_timer_get_time();
    if (now - s_report_us >= 10000000) {
        ESP_LOGI(TAG, "preview: %lu updates in %lu ms", (unsigned long)s_frames,
                 (unsigned long)((now - s_report_us) / 1000));
        s_frames = 0;
        s_report_us = now;
    }
}

#else

void capture_dsi_preview_init(void) {}
bool capture_dsi_preview_due(void) { return false; }
void capture_dsi_preview_frame(const void *src, uint32_t width, uint32_t height,
                               const capture_pixfmt_t *fmt)
{
    (void)src; (void)width; (void)height; (void)fmt;
}

#endif
