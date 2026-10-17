# SPDX-License-Identifier: Apache-2.0
"""Exercise real WireGuard lifecycle functions with allocation faults and races."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[2] / 'kvm_net/kvm_wg.c').read_text()
start = source.index('static esp_err_t start_task(void)')
end = source.index('/* Resolve "host"', start)
a = source.index('esp_err_t kvm_wg_apply(void)')
b = source.index('\nvoid kvm_wg_status', a)
harness = r'''
#include <pthread.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int esp_err_t;
typedef pthread_mutex_t *SemaphoreHandle_t;
typedef void *TaskHandle_t;
#define ESP_OK 0
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_INVALID_STATE 2
#define pdPASS 1
#define portMAX_DELAY -1
#define IP_EVENT 1
#define IP_EVENT_ETH_GOT_IP 2
#define TAG "test"
#define ESP_LOGW(...) ((void)0)
static SemaphoreHandle_t s_mtx, s_apply_sem;
static TaskHandle_t s_task;
static bool enabled, fail_task;
static int allocations, fail_allocation, live, creates, registered;
static SemaphoreHandle_t new_sem(void) {
    if (++allocations == fail_allocation) return NULL;
    SemaphoreHandle_t s = malloc(sizeof(*s)); assert(s);
    pthread_mutex_init(s, NULL); live++; return s;
}
#define xSemaphoreCreateMutex new_sem
#define xSemaphoreCreateBinary new_sem
static void vSemaphoreDelete(SemaphoreHandle_t s) { pthread_mutex_destroy(s); free(s); live--; }
static int xSemaphoreTake(SemaphoreHandle_t s, int ticks) {
    (void)ticks; assert(s == s_mtx); return pthread_mutex_lock(s) == 0;
}
static void xSemaphoreGive(SemaphoreHandle_t s) {
    if (s == s_mtx) pthread_mutex_unlock(s); /* binary apply signal is coalesced */
}
static bool kvm_setting_bool(const char *key) { assert(!strcmp(key,"wg_enable")); return enabled; }
static void wg_task(void *arg) { (void)arg; }
static void on_got_ip(void) {}
static int esp_event_handler_register(int event, int id, void (*cb)(void), void *arg) {
    (void)event; (void)id; (void)cb; (void)arg; registered++; return ESP_OK;
}
static int xTaskCreate(void (*fn)(void *), const char *name, int stack, void *arg, int priority, TaskHandle_t *out) {
    assert(fn == wg_task && !strcmp(name,"kvm_wg") && stack == 8192 && !arg && priority == 5);
    creates++;
    if (fail_task) return 0;
    *out = (void *)1; return pdPASS;
}
esp_err_t kvm_wg_apply(void);
'''+source[start:end]+source[a:b]+r'''
static void *apply(void *unused) { (void)unused; assert(kvm_wg_apply() == ESP_OK); return NULL; }
static void reset(void) {
    if(s_mtx) vSemaphoreDelete(s_mtx);
    if(s_apply_sem) vSemaphoreDelete(s_apply_sem);
    s_mtx = s_apply_sem = NULL; s_task = NULL;
    assert(live == 0); allocations = creates = registered = fail_allocation = 0;
    enabled = fail_task = false;
}
int main(void) {
    assert(kvm_wg_apply() == ESP_ERR_INVALID_STATE);
    for (int fail = 1; fail <= 2; fail++) {
        fail_allocation = fail;
        assert(kvm_wg_init() == ESP_ERR_NO_MEM);
        assert(!s_mtx && !s_apply_sem && live == 0 && registered == 0);
        fail_allocation = 0;
        assert(kvm_wg_init() == ESP_OK);
        assert(creates == 0 && live == 2);
        reset();
    }
    assert(kvm_wg_init() == ESP_OK && creates == 0);
    assert(kvm_wg_init() == ESP_OK && live == 2 && registered == 1);
    enabled = true; fail_task = true;
    assert(kvm_wg_apply() == ESP_ERR_NO_MEM && !s_task);
    fail_task = false;
    pthread_t callers[16];
    for (int i = 0; i < 16; i++) assert(!pthread_create(&callers[i],NULL,apply,NULL));
    for (int i = 0; i < 16; i++) pthread_join(callers[i],NULL);
    assert(creates == 2 && s_task); /* one failed attempt, exactly one success */
    enabled = false; assert(kvm_wg_apply() == ESP_OK && creates == 2);
    enabled = true; assert(kvm_wg_apply() == ESP_OK && creates == 2);
    reset();
    enabled = true; assert(kvm_wg_init() == ESP_OK && creates == 1);
    reset();
    puts("WireGuard lazy worker: allocation recovery, first enable and concurrent apply passed");
}
'''
with tempfile.TemporaryDirectory(prefix='espkvm-wg-test-') as tmp:
    c = Path(tmp) / 'test.c'; exe = Path(tmp) / 'test'
    c.write_text(harness)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-pthread','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
