/* SPDX-License-Identifier: Apache-2.0 */
#include "sdkconfig.h"
#include "xbox_ble.h"
#if CONFIG_KVM_XBOX_BLE
#include <stdatomic.h>
#include <string.h>
#include "esp_log.h"
#include "esp_hosted_bt_host_stack.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* First establish the installed C6's HCI path before adding bonding and HID
 * subscriptions. Never auto-update or reset its working Wi-Fi firmware. */
enum { OFF, STARTING, SCANNING, FOUND, FAILED, TIMEOUT };
static atomic_uint s_state;
static atomic_bool s_requested, s_synced;
static uint8_t s_addr_type;
static const char *TAG="xbox_ble";
const char *xbox_ble_status(void)
{
    static const char *text[]={"BLUETOOTH IDLE","STARTING BLUETOOTH","SEARCHING: HOLD PAIR",
        "XBOX FOUND - HID SETUP NEXT","BLUETOOTH INIT FAILED","NO CONTROLLER - REENTER TO RETRY"};
    return text[atomic_load(&s_state)];
}
void xbox_ble_mode(bool pairing) { atomic_store(&s_requested,pairing); }
static int gap_event(struct ble_gap_event *event,void *arg)
{
    (void)arg;
    if(event->type==BLE_GAP_EVENT_DISC) {
        struct ble_hs_adv_fields fields;
        if(!ble_hs_adv_parse_fields(&fields,event->disc.data,event->disc.length_data) &&
           fields.name && fields.name_len>=4 && !memcmp(fields.name,"Xbox",4)) {
            ESP_LOGI(TAG,"Xbox advertisement received, RSSI=%d",event->disc.rssi);
            atomic_store(&s_state,FOUND);
        }
    } else if(event->type==BLE_GAP_EVENT_DISC_COMPLETE && atomic_load(&s_state)==SCANNING)
        atomic_store(&s_state,TIMEOUT);
    return 0;
}
static void on_sync(void)
{
    int rc=ble_hs_util_ensure_addr(0);
    if(!rc) rc=ble_hs_id_infer_auto(0,&s_addr_type);
    ESP_LOGI(TAG,"HCI synchronized, address result=%d",rc);
    atomic_store(&s_synced,rc==0);
    if(rc) atomic_store(&s_state,FAILED);
}
static void on_reset(int reason)
{
    atomic_store(&s_synced,false);
    atomic_store(&s_state,FAILED);
    ESP_LOGW(TAG,"Bluetooth host reset: %d",reason);
}
static void host_task(void *arg) { (void)arg; nimble_port_run(); nimble_port_freertos_deinit(); }
static void worker(void *arg)
{
    (void)arg;
    bool attempted=false,was_requested=false;
    for(;;) {
        bool requested=atomic_load(&s_requested);
        if(requested && !attempted) {
            attempted=true;
            atomic_store(&s_state,STARTING);
            esp_hosted_bt_host_stack_cfg_t cfg=ESP_HOSTED_BT_HOST_STACK_CONFIG_DEFAULT();
            /* Installed legacy CP advertises HCI and starts its controller
             * itself. New CP lifecycle RPCs are not supported by that image. */
            cfg.bring_up_controller=!CONFIG_KVM_XBOX_BLE_LEGACY_CP;
            esp_err_t err=esp_hosted_bt_host_stack_setup(&cfg);
            if(err==ESP_OK) err=nimble_port_init();
            if(err==ESP_OK) {
                ble_hs_cfg.sync_cb=on_sync;
                ble_hs_cfg.reset_cb=on_reset;
                nimble_port_freertos_init(host_task);
                for(unsigned i=0;i<50 && !atomic_load(&s_synced);i++) vTaskDelay(pdMS_TO_TICKS(100));
            }
            if(err!=ESP_OK || !atomic_load(&s_synced)) {
                ESP_LOGE(TAG,"Radio setup failed: %s synced=%d",esp_err_to_name(err),atomic_load(&s_synced));
                atomic_store(&s_state,FAILED);
            }
        }
        if(requested && !was_requested && atomic_load(&s_synced)) {
            struct ble_gap_disc_params p={.itvl=160,.window=80,.filter_duplicates=1,.passive=0};
            int rc=ble_gap_disc(s_addr_type,30000,&p,gap_event,NULL);
            ESP_LOGI(TAG,"BLE scan result=%d",rc);
            atomic_store(&s_state,rc?FAILED:SCANNING);
        }
        if(!requested && was_requested && atomic_load(&s_synced)) {
            ble_gap_disc_cancel();
            atomic_store(&s_state,OFF);
        }
        was_requested=requested;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void xbox_ble_init(void)
{
    if(xTaskCreate(worker,"xbox_ble",6144,NULL,3,NULL)!=pdPASS) atomic_store(&s_state,FAILED);
}
#else
void xbox_ble_mode(bool pairing) { (void)pairing; }
const char *xbox_ble_status(void) { return "BLUETOOTH NOT BUILT"; }
void xbox_ble_init(void) {}
#endif
