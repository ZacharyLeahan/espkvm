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
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "usb_xbox.h"
#include "xbox_ble_report.h"
#include "xbox_ble_crypto_test.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Use the installed C6's HCI path; never auto-update its Wi-Fi firmware.
 * Pair only during the explicit local Bluetooth mode, one controller at a time. */
enum { OFF, STARTING, SCANNING, FOUND, FAILED, TIMEOUT, PAIRING, HID, READY, PAIR_FAILED, ACTIVE, RETRYING };
static atomic_uint s_state;
static atomic_bool s_requested, s_synced;
static atomic_uint s_conn=BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_start,s_end,s_input,s_map;
static unsigned s_map_bytes;
static struct { uint16_t value,end,cccd,ref; } s_reports[8];
static unsigned s_count,s_index;
static bool s_discovering;
static atomic_uint s_notifications,s_valid_reports;
static atomic_bool s_retry;
static bool s_repaired_bond;
void ble_store_config_init(void);
static uint8_t s_addr_type;
static const char *TAG="xbox_ble";
static void retry_link(int reason)
{
    if(atomic_load(&s_requested) &&
       (reason==BLE_HS_HCI_ERR(BLE_ERR_CONN_SPVN_TMO) ||
        reason==BLE_HS_HCI_ERR(BLE_ERR_CONN_ESTABLISHMENT) ||
        reason==BLE_HS_HCI_ERR(BLE_ERR_REM_USER_CONN_TERM)))
        atomic_store(&s_retry,true);
}
const char *xbox_ble_status(void)
{
    static const char *text[]={"BLUETOOTH IDLE","STARTING BLUETOOTH","SEARCHING: HOLD PAIR",
        "CONNECTING XBOX","BLUETOOTH ERROR - RETRY","NO CONTROLLER - REENTER TO RETRY",
        "PAIRING XBOX","READING CONTROLLER","CONNECTED - WAITING FOR INPUT","PAIRING REJECTED - RETRY",
        "CONTROLLER INPUT ACTIVE","LINK LOST - RETRYING"};
    return text[atomic_load(&s_state)];
}
void xbox_ble_mode(bool pairing) { atomic_store(&s_requested,pairing); }
static void failed(const char *stage,int rc)
{
    ESP_LOGW(TAG,"%s failed: %d",stage,rc);
    atomic_store(&s_state,rc==BLE_HS_SM_PEER_ERR(BLE_SM_ERR_DHKEY)?PAIR_FAILED:FAILED);
    usb_xbox_bluetooth(NULL);
    unsigned c=atomic_load(&s_conn);
    /* ENC_CHANGE can report ENOTCONN while GAP is delivering a disconnect.
     * Do not issue another HCI disconnect against that vanished connection. */
    if(c!=BLE_HS_CONN_HANDLE_NONE && rc!=BLE_HS_ENOTCONN)
        ble_gap_terminate(c,BLE_ERR_REM_USER_CONN_TERM);
}
static void next_report(void);
static int subscribed(uint16_t c,const struct ble_gatt_error *e,struct ble_gatt_attr *a,void *arg)
{
    (void)c;(void)a;(void)arg;
    if(e->status) failed("subscribe",e->status);
    else {
        ESP_LOGI(TAG,"HID subscribed: value=%u cccd=%u",s_reports[s_index].value,s_reports[s_index].cccd);
        ++s_index; next_report();
    }
    return 0;
}
static int report_ref(uint16_t c,const struct ble_gatt_error *e,struct ble_gatt_attr *a,void *arg)
{
    (void)arg;
    uint8_t ref[2];
    if(e->status || !a || OS_MBUF_PKTLEN(a->om)!=2 || os_mbuf_copydata(a->om,0,2,ref)) {
        failed("report reference",e->status); return 0;
    }
    ESP_LOGI(TAG,"HID report value=%u id=%u type=%u",s_reports[s_index].value,ref[0],ref[1]);
    if(ref[1]==1 && s_reports[s_index].cccd) {
        if(ref[0]==1) s_input=s_reports[s_index].value;
        uint8_t enable[2]={1,0};
        int rc=ble_gattc_write_flat(c,s_reports[s_index].cccd,enable,2,subscribed,NULL);
        if(rc) failed("enable notifications",rc);
    } else { ++s_index; next_report(); }
    return 0;
}
static int descriptors(uint16_t c,const struct ble_gatt_error *e,uint16_t value,
                       const struct ble_gatt_dsc *d,void *arg)
{
    (void)value;(void)arg;
    if(!e->status) {
        unsigned uuid=ble_uuid_u16(&d->uuid.u);
        if(uuid==0x2902) s_reports[s_index].cccd=d->handle;
        if(uuid==0x2908) s_reports[s_index].ref=d->handle;
    } else if(e->status==BLE_HS_EDONE) {
        if(s_reports[s_index].ref) {
            int rc=ble_gattc_read(c,s_reports[s_index].ref,report_ref,NULL);
            if(rc) failed("read reference",rc);
        } else { ++s_index; next_report(); }
    } else failed("descriptors",e->status);
    return 0;
}
static void next_report(void)
{
    if(s_index>=s_count) {
        if(!s_input) failed("supported HID input missing",0);
        else { atomic_store(&s_state,READY); ESP_LOGI(TAG,"All HID input reports enabled; gamepad handle=%u",s_input); }
        return;
    }
    int rc=ble_gattc_disc_all_dscs(atomic_load(&s_conn),s_reports[s_index].value,
                                 s_reports[s_index].end,descriptors,NULL);
    if(rc) failed("discover descriptors",rc);
}
static int report_map(uint16_t c,const struct ble_gatt_error *e,struct ble_gatt_attr *a,void *arg)
{
    (void)c;(void)arg;
    if(!e->status && a) {
        s_map_bytes+=OS_MBUF_PKTLEN(a->om);
        if(s_map_bytes>2048) { failed("HID map too large",0); return BLE_HS_EAPP; }
    } else if(e->status==BLE_HS_EDONE) {
        ESP_LOGI(TAG,"HID report map read: %u bytes",s_map_bytes);
        s_index=0; next_report();
    } else failed("read report map",e->status);
    return 0;
}
static int characteristics(uint16_t c,const struct ble_gatt_error *e,const struct ble_gatt_chr *ch,void *arg)
{
    (void)c;(void)arg;
    if(!e->status) {
        ESP_LOGI(TAG,"HID characteristic uuid=%04x value=%u props=%02x",ble_uuid_u16(&ch->uuid.u),ch->val_handle,ch->properties);
        if(ble_uuid_u16(&ch->uuid.u)==0x2a4b) s_map=ch->val_handle;
        if(s_count && !s_reports[s_count-1].end) s_reports[s_count-1].end=ch->def_handle-1;
        if(ble_uuid_u16(&ch->uuid.u)==0x2a4d && (ch->properties & BLE_GATT_CHR_PROP_NOTIFY)) {
            if(s_count==8) { failed("too many HID reports",0); return 0; }
            s_reports[s_count++].value=ch->val_handle;
        }
    } else if(e->status==BLE_HS_EDONE) {
        if(s_count && !s_reports[s_count-1].end) s_reports[s_count-1].end=s_end;
        if(s_map) {
            int rc=ble_gattc_read_long(c,s_map,0,report_map,NULL);
            if(rc) failed("read HID map",rc);
        } else failed("HID map missing",0);
    } else failed("characteristics",e->status);
    return 0;
}
static int service(uint16_t c,const struct ble_gatt_error *e,const struct ble_gatt_svc *svc,void *arg)
{
    (void)arg;
    if(!e->status) { s_start=svc->start_handle; s_end=svc->end_handle; }
    else if(e->status==BLE_HS_EDONE && s_start) {
        int rc=ble_gattc_disc_all_chrs(c,s_start,s_end,characteristics,NULL);
        if(rc) failed("discover characteristics",rc);
    } else failed("HID service",e->status);
    return 0;
}
static int gap_event(struct ble_gap_event *event,void *arg)
{
    (void)arg;
    if(event->type==BLE_GAP_EVENT_DISC) {
        struct ble_hs_adv_fields fields;
        if(!ble_hs_adv_parse_fields(&fields,event->disc.data,event->disc.length_data) &&
           fields.name && fields.name_len==24 && !memcmp(fields.name,"Xbox Wireless Controller",24) &&
           atomic_load(&s_requested) && atomic_load(&s_state)==SCANNING) {
            ESP_LOGI(TAG,"Xbox advertisement received, RSSI=%d",event->disc.rssi);
            atomic_store(&s_state,FOUND);
            ble_gap_disc_cancel();
            /* Keep the existing 30-50 ms interval, but allow more tolerance for
             * the shared Wi-Fi/BLE radio than the default 2.56 s timeout. */
            struct ble_gap_conn_params p={.scan_itvl=16,.scan_window=16,
                .itvl_min=24,.itvl_max=40,.supervision_timeout=600};
            int rc=ble_gap_connect(s_addr_type,&event->disc.addr,10000,&p,gap_event,NULL);
            if(rc) failed("connect",rc);
        }
    } else if(event->type==BLE_GAP_EVENT_DISC_COMPLETE && atomic_load(&s_state)==SCANNING)
        atomic_store(&s_state,TIMEOUT);
    else if(event->type==BLE_GAP_EVENT_CONNECT) {
        if(event->connect.status) {
            failed("connection",event->connect.status);
            retry_link(event->connect.status);
            return 0;
        }
        atomic_store(&s_conn,event->connect.conn_handle);
        ESP_LOGI(TAG,"GAP connected: handle=%u",event->connect.conn_handle);
        if(!atomic_load(&s_requested)) { ble_gap_terminate(event->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM); return 0; }
        s_start=s_end=s_input=s_map=0; s_count=s_index=s_map_bytes=0; s_discovering=false;
        s_repaired_bond=false;
        atomic_store(&s_notifications,0); atomic_store(&s_valid_reports,0);
        memset(s_reports,0,sizeof(s_reports));
        atomic_store(&s_state,PAIRING);
        int rc=ble_gap_security_initiate(event->connect.conn_handle);
        ESP_LOGI(TAG,"Security initiation result=%d",rc);
        if(rc && rc!=BLE_HS_EALREADY) failed("encryption",rc);
    } else if(event->type==BLE_GAP_EVENT_REPEAT_PAIRING) {
        struct ble_gap_conn_desc desc;
        const struct ble_gap_repeat_pairing *p=&event->repeat_pairing;
        /* Explicit local pairing mode authorizes one replacement for this
         * peer only, without accepting weaker security or erasing all bonds. */
        if(!atomic_load(&s_requested) || p->conn_handle!=atomic_load(&s_conn) ||
           s_repaired_bond || !p->new_sc || p->new_key_size!=16 ||
           (p->cur_authenticated && !p->new_authenticated) ||
           ble_gap_conn_find(p->conn_handle,&desc)) return BLE_GAP_REPEAT_PAIRING_IGNORE;
        int rc=ble_store_util_delete_peer(&desc.peer_id_addr);
        ESP_LOGI(TAG,"Controller requested fresh pairing: old bond removal=%d",rc);
        if(rc) return BLE_GAP_REPEAT_PAIRING_IGNORE;
        s_repaired_bond=true;
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    } else if(event->type==BLE_GAP_EVENT_ENC_CHANGE) {
        struct ble_gap_conn_desc desc;
        if(event->enc_change.status || ble_gap_conn_find(event->enc_change.conn_handle,&desc) || !desc.sec_state.encrypted) {
            failed("pairing",event->enc_change.status); return 0;
        }
        if(!s_discovering) {
            ESP_LOGI(TAG,"Encrypted link: key size=%u bonded=%u",desc.sec_state.key_size,desc.sec_state.bonded);
            s_discovering=true; atomic_store(&s_state,HID);
            int rc=ble_gattc_disc_svc_by_uuid(event->enc_change.conn_handle,BLE_UUID16_DECLARE(0x1812),service,NULL);
            if(rc) failed("discover HID",rc);
        }
    } else if(event->type==BLE_GAP_EVENT_NOTIFY_RX && atomic_load(&s_requested) &&
              event->notify_rx.conn_handle==atomic_load(&s_conn)) {
        uint8_t input[16],output[20];
        unsigned n=OS_MBUF_PKTLEN(event->notify_rx.om);
        unsigned count=atomic_fetch_add(&s_notifications,1)+1;
        if(count<=8) ESP_LOGI(TAG,"HID notification #%u handle=%u len=%u",count,event->notify_rx.attr_handle,n);
        if(event->notify_rx.attr_handle!=s_input ||
           (atomic_load(&s_state)!=READY && atomic_load(&s_state)!=ACTIVE)) return 0;
        if(n!=16 || os_mbuf_copydata(event->notify_rx.om,0,16,input) || !xbox_ble_report(input,n,output)) {
            failed("unsupported report layout",n); return 0;
        }
        usb_xbox_bluetooth(output);
        if(atomic_fetch_add(&s_valid_reports,1)==0) ESP_LOGI(TAG,"First valid controller input forwarded to USB");
        atomic_store(&s_state,ACTIVE);
    } else if(event->type==BLE_GAP_EVENT_DISCONNECT) {
        atomic_store(&s_conn,BLE_HS_CONN_HANDLE_NONE);
        usb_xbox_bluetooth(NULL);
        if(!atomic_load(&s_requested)) atomic_store(&s_state,OFF);
        else if(atomic_load(&s_state)!=PAIR_FAILED) atomic_store(&s_state,FAILED);
        ESP_LOGI(TAG,"Controller disconnected: %d",event->disconnect.reason);
        retry_link(event->disconnect.reason);
    }
    return 0;
}
static void on_sync(void)
{
    /* Xbox BLE reference uses RANDOM rather than the controller's public
     * address. Keep one random-static identity for this diagnostic boot. */
    static ble_addr_t identity;
    static bool have_identity;
    int rc=0;
    if(!have_identity) {
        rc=ble_hs_id_gen_rnd(0,&identity);
        if(!rc) have_identity=true;
    }
    if(!rc) rc=ble_hs_id_set_rnd(identity.val);
    s_addr_type=BLE_OWN_ADDR_RANDOM;
    if(!rc) {
        bool valid=xbox_ble_crypto_selftest();
        ESP_LOGI(TAG,"Secure Connections DHKey known-answer test: %s",valid?"PASS":"FAIL");
        if(!valid) rc=BLE_HS_EUNKNOWN;
    }
    ESP_LOGI(TAG,"HCI synchronized, random-static address result=%d",rc);
    atomic_store(&s_synced,rc==0);
    if(rc) atomic_store(&s_state,FAILED);
}
static void on_reset(int reason)
{
    atomic_store(&s_synced,false);
    atomic_store(&s_state,FAILED);
    usb_xbox_bluetooth(NULL);
    ESP_LOGW(TAG,"Bluetooth host reset: %d",reason);
}
static void host_task(void *arg) { (void)arg; nimble_port_run(); nimble_port_freertos_deinit(); }
static void worker(void *arg)
{
    (void)arg;
    bool attempted=false,was_requested=false;
    unsigned setup_ticks=0,diagnostic_ticks=0,retry_ticks=0,retries=0;
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
                ble_hs_cfg.sm_io_cap=BLE_SM_IO_CAP_NO_IO;
                ble_hs_cfg.sm_bonding=1;
                /* Match the Xbox BLE reference's auth-request bits. NoIO still
                 * negotiates Just Works; this is not a claim of MITM protection. */
                ble_hs_cfg.sm_mitm=1;
                ble_hs_cfg.sm_sc=1;
                ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
                ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
                ble_store_config_init();
                nimble_port_freertos_init(host_task);
                for(unsigned i=0;i<50 && !atomic_load(&s_synced);i++) vTaskDelay(pdMS_TO_TICKS(100));
            }
            if(err!=ESP_OK || !atomic_load(&s_synced)) {
                ESP_LOGE(TAG,"Radio setup failed: %s synced=%d",esp_err_to_name(err),atomic_load(&s_synced));
                atomic_store(&s_state,FAILED);
            }
        }
        if(!requested) {
            atomic_store(&s_retry,false);
            retry_ticks=0; retries=0;
        } else if(atomic_exchange(&s_retry,false) && retries<3) {
            ++retries; retry_ticks=20;
            atomic_store(&s_state,RETRYING);
            ESP_LOGI(TAG,"Link recovery %u/3 scheduled",retries);
        }
        bool retry_due=retry_ticks && --retry_ticks==0;
        if(requested && (!was_requested || retry_due) && atomic_load(&s_synced) &&
           atomic_load(&s_conn)==BLE_HS_CONN_HANDLE_NONE) {
            struct ble_gap_disc_params p={.itvl=160,.window=80,.filter_duplicates=1,.passive=0};
            atomic_store(&s_state,SCANNING);
            int rc=ble_gap_disc(s_addr_type,30000,&p,gap_event,NULL);
            ESP_LOGI(TAG,"BLE scan result=%d",rc);
            if(rc) atomic_store(&s_state,FAILED);
        }
        if(!requested && was_requested && atomic_load(&s_synced)) {
            ble_gap_disc_cancel();
            ble_gap_conn_cancel();
            unsigned c=atomic_load(&s_conn);
            if(c!=BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(c,BLE_ERR_REM_USER_CONN_TERM);
            usb_xbox_bluetooth(NULL);
            atomic_store(&s_state,OFF);
        }
        unsigned state=atomic_load(&s_state);
        if(requested && ++diagnostic_ticks>=100) {
            diagnostic_ticks=0;
            ESP_LOGI(TAG,"state=%u notifications=%u forwarded=%u",state,atomic_load(&s_notifications),atomic_load(&s_valid_reports));
        }
        if(state==FOUND || state==PAIRING || state==HID) {
            if(++setup_ticks>=300) { failed("pairing/HID timeout",BLE_HS_ETIMEOUT); setup_ticks=0; }
        } else setup_ticks=0;
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
