/* SPDX-License-Identifier: Apache-2.0
 * Independent TinyUSB XID implementation. Protocol references and provenance:
 * docs/XBOX-REMOTE-CONTROLLER.md. Experimental, one original-Xbox controller.
 */
#include "sdkconfig.h"
#if CONFIG_KVM_USB_XBOX
#include "usb_xbox.h"
#include "xbox_report.h"
#include "device/usbd_pvt.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "soc/usb_dwc_struct.h"

static const char *TAG = "usb_xbox";
static QueueHandle_t s_queue;
static SemaphoreHandle_t s_input_mu;
static int64_t s_local_until;
static int64_t s_bluetooth_until;
static uint8_t s_port;
static bool s_open;
static int64_t s_deadline;
static uint8_t s_state[20] = {0,20};
/* P4 USB DMA cache maintenance requires separate, whole cache-line buffers. */
CFG_TUSB_MEM_SECTION static uint8_t s_tx[64];
CFG_TUSB_MEM_SECTION static uint8_t s_control[64];
CFG_TUSB_MEM_SECTION static uint8_t s_out[64];
static uint32_t s_sent;

static const tusb_desc_device_t device = {
    .bLength=18, .bDescriptorType=TUSB_DESC_DEVICE, .bcdUSB=0x0110,
    .bMaxPacketSize0=64, .idVendor=0x045e, .idProduct=0x0289,
    .bcdDevice=0x0121, .bNumConfigurations=1,
};
/* XID uses class 0x58, not USB HID class 3. No keyboard or storage interfaces. */
static const uint8_t configuration[] = {
    9,2,32,0,1,1,0,0x80,50,
    9,4,0,0,2,0x58,0x42,0,0,
    7,5,0x81,3,32,0,4,
    7,5,0x02,3,32,0,4,
};
static const uint8_t xid_descriptor[16] = {
    16,0x42,0,1,1,2,20,6,255,255,255,255,255,255,255,255
};

static void queue_release(void)
{
    if (!s_queue) return;
    uint8_t neutral[20] = {0,20};
    xQueueReset(s_queue);
    (void)xQueueSend(s_queue, neutral, 0);
}

void usb_xbox_release(void)
{
    if (!s_input_mu || xSemaphoreTake(s_input_mu,pdMS_TO_TICKS(10)) != pdTRUE) return;
    if (esp_timer_get_time() >= s_local_until && esp_timer_get_time() >= s_bluetooth_until) queue_release();
    xSemaphoreGive(s_input_mu);
}

void usb_xbox_keyboard(const uint8_t keys[6])
{
    if (!s_queue || !keys) return;
    if (xSemaphoreTake(s_input_mu,pdMS_TO_TICKS(10)) != pdTRUE) return;
    if (esp_timer_get_time() < s_local_until || esp_timer_get_time() < s_bluetooth_until) { xSemaphoreGive(s_input_mu); return; }
    uint8_t report[20];
    xbox_keyboard_report(keys, report);
    /* Saturation must never lose a release and leave a held button. */
    if (xQueueSend(s_queue, report, 0) != pdTRUE) queue_release();
    xSemaphoreGive(s_input_mu);
}

/* Physical touch has priority while fresh; never mix local and remote states. */
void usb_xbox_touch(const uint8_t keys[6], const int16_t axes[4], bool active)
{
    if (!s_queue || !keys || xSemaphoreTake(s_input_mu,pdMS_TO_TICKS(10)) != pdTRUE) return;
    s_local_until=active ? esp_timer_get_time()+300000 : 0;
    uint8_t report[20];
    xbox_keyboard_report(keys,report);
    xbox_report_axes(report,axes);
    xQueueReset(s_queue);
    (void)xQueueSend(s_queue,report,0);
    xSemaphoreGive(s_input_mu);
}

void usb_xbox_bluetooth(const uint8_t report[20])
{
    if (!s_queue || xSemaphoreTake(s_input_mu,pdMS_TO_TICKS(10)) != pdTRUE) return;
    s_bluetooth_until=report ? esp_timer_get_time()+300000 : 0;
    if (esp_timer_get_time() >= s_local_until) {
        if (report) { xQueueReset(s_queue); (void)xQueueSend(s_queue,report,0); }
        else queue_release();
    }
    xSemaphoreGive(s_input_mu);
}

static void xid_reset(uint8_t port)
{
    s_port = port;
    s_open = false;
    memset(s_state, 0, sizeof(s_state));
    s_state[1] = 20;
    s_deadline = 0;
    if (s_queue) xQueueReset(s_queue);
}
static void xid_init(void) { xid_reset(0); }
static bool xid_deinit(void) { xid_reset(0); return true; }

static uint16_t xid_open(uint8_t port, tusb_desc_interface_t const *itf, uint16_t length)
{
    if (itf->bInterfaceClass != 0x58 || itf->bInterfaceSubClass != 0x42 ||
        itf->bInterfaceNumber != 0 || itf->bNumEndpoints != 2 || length < 23) return 0;
    const tusb_desc_endpoint_t *in = (const void *)((const uint8_t *)itf + 9);
    const tusb_desc_endpoint_t *out = (const void *)((const uint8_t *)itf + 16);
    if (!usbd_edpt_open(port, in) || !usbd_edpt_open(port, out)) return 0;
    s_port = port;
    s_open = true;
    if (!usbd_edpt_xfer(port, 0x02, s_out, 32, false)) return 0;
    ESP_LOGI(TAG, "XID interface opened on USB port %u", port);
    return 23;
}

static bool xid_control(uint8_t port, uint8_t stage, tusb_control_request_t const *r)
{
    if (stage != CONTROL_STAGE_SETUP) return true;
    if (r->wIndex != 0) return false;
    ESP_LOGI(TAG, "control %02x/%02x value=%04x length=%u", r->bmRequestType,
             r->bRequest, r->wValue, r->wLength);
    if (r->bmRequestType == 0xc1 && r->bRequest == 6 && r->wValue == 0x4200)
        return tud_control_xfer(port,r,(void *)xid_descriptor,sizeof(xid_descriptor));
    if (r->bmRequestType == 0xc1 && r->bRequest == 1 &&
        (r->wValue == 0x0100 || r->wValue == 0x0200)) {
        unsigned n = r->wValue == 0x0100 ? 20 : 6;
        memset(s_control,255,n);
        s_control[0]=0; s_control[1]=n;
        if (n == 20) s_control[3]=0;
        return tud_control_xfer(port,r,s_control,n);
    }
    if (r->bmRequestType == 0xa1 && r->bRequest == 1 && r->wValue == 0x0100) {
        memcpy(s_control,s_state,20);
        return tud_control_xfer(port,r,s_control,20);
    }
    /* Consume motor reports, but no physical rumble actuator is implemented. */
    if (r->bmRequestType == 0x21 && r->bRequest == 9 && r->wValue == 0x0200 && r->wLength == 6)
        return tud_control_xfer(port,r,s_control,6);
    return false;
}
bool tud_vendor_control_xfer_cb(uint8_t p,uint8_t stage,tusb_control_request_t const *r)
{ return xid_control(p,stage,r); }

static bool xid_xfer(uint8_t port,uint8_t ep,xfer_result_t result,uint32_t bytes)
{
    (void)bytes;
    if (ep == 0x02 && s_open) return usbd_edpt_xfer(port,ep,s_out,32,false);
    if (ep == 0x81 && result == XFER_RESULT_SUCCESS && ++s_sent == 1)
        ESP_LOGI(TAG,"Xbox host consumed first controller report");
    return true;
}
static const usbd_class_driver_t driver = {
    .name="XID", .init=xid_init, .deinit=xid_deinit, .reset=xid_reset,
    .open=xid_open, .control_xfer_cb=xid_control, .xfer_cb=xid_xfer,
};
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *count)
{ *count=1; return &driver; }

/* All endpoint operations and mutable USB state stay on TinyUSB's task.
 * Keep reports ordered (including immediate press/release pairs). */
static void xid_pump(void *unused)
{
    (void)unused;
    if (!s_open || !tud_mounted() || tud_suspended()) {
        usb_xbox_release();
        memset(s_state,0,20); s_state[1]=20;
        return;
    }
    if (!usbd_edpt_claim(s_port,0x81)) return;
    if (xQueueReceive(s_queue,s_state,0) == pdTRUE) s_deadline=esp_timer_get_time()+500000;
    if (esp_timer_get_time() >= s_deadline) {
        memset(s_state,0,20); s_state[1]=20;
    }
    memcpy(s_tx,s_state,20);
    if (!usbd_edpt_xfer(s_port,0x81,s_tx,20,false)) usbd_edpt_release(s_port,0x81);
}
static void xid_worker(void *arg)
{
    (void)arg;
    for (;;) {
        usbd_defer_func(xid_pump,NULL,false);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t usb_xbox_init(void)
{
    if (s_queue) return ESP_OK;
    esp_err_t err=gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    s_queue=xQueueCreate(16,20);
    if (!s_queue) return ESP_ERR_NO_MEM;
    s_input_mu=xSemaphoreCreateMutex();
    if (!s_input_mu) return ESP_ERR_NO_MEM;
    tinyusb_config_t cfg=TINYUSB_DEFAULT_CONFIG();
    cfg.descriptor.device=&device;
    cfg.descriptor.full_speed_config=configuration;
#if TUD_OPT_HIGH_SPEED
    cfg.descriptor.high_speed_config=configuration;
#endif
    err=tinyusb_driver_install(&cfg);
    if (err != ESP_OK) return err;
    tud_disconnect();
#if TUD_OPT_HIGH_SPEED
    USB_DWC_HS.dcfg_reg.devspd=1; /* Original Xbox USB 1.1, same physical DEVICE port. */
#endif
    vTaskDelay(pdMS_TO_TICKS(1000));
    tud_connect();
    if (xTaskCreate(xid_worker,"xid_input",3072,NULL,8,NULL) != pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG,"Experimental Xbox XID mode; inputs neutral, stale keys release after 500 ms");
    return ESP_OK;
}
#endif
