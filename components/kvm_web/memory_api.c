/* SPDX-License-Identifier: Apache-2.0 */
#include "sdkconfig.h"
#include "web_priv.h"
#include "kvm_auth.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#if CONFIG_FREERTOS_USE_TRACE_FACILITY
#include "freertos/freertos_debug.h"
#if !CONFIG_FREERTOS_UNICORE
#include "esp_ipc.h"
#endif

#define MEMORY_TASK_LIMIT 128

typedef struct {
    char name[configMAX_TASK_NAME_LEN];
    uint32_t priority, stack, free_min;
    bool external;
} memory_task_t;

typedef struct {
    TaskStatus_t *status;
    memory_task_t *tasks;
    SemaphoreHandle_t done;
    atomic_bool remote_ready, remote_release, remote_done;
    UBaseType_t count;
    esp_err_t result;
    int64_t snapshot_us;
} memory_snapshot_t;

#if !CONFIG_FREERTOS_UNICORE
static void hold_remote_scheduler(void *arg)
{
    memory_snapshot_t *s = arg;
    vTaskSuspendAll();
    atomic_store(&s->remote_ready, true);
    while (!atomic_load(&s->remote_release)) { /* caller copies bounded task metadata */ }
    (void)xTaskResumeAll();
    atomic_store(&s->remote_done, true);
}
#endif

/* A pinned worker avoids migrating onto the core whose scheduler we suspend.
 * Allocate before entry; no heap, logging, JSON or network calls while paused.
 * IDF's task snapshot API requires suspended schedulers. Suspending only this
 * core would let the other idle task free a TCB/name while it is being copied. */
static void snapshot_worker(void *arg)
{
    memory_snapshot_t *s = arg;
#if !CONFIG_FREERTOS_UNICORE
    s->result = esp_ipc_call(1, hold_remote_scheduler, s);
    if (s->result != ESP_OK) goto done;
    while (!atomic_load(&s->remote_ready)) { }
#endif
    vTaskSuspendAll();
    const int64_t started = esp_timer_get_time();
    s->count = uxTaskGetSystemState(s->status, MEMORY_TASK_LIMIT, NULL);
    for (UBaseType_t i = 0; i < s->count; i++) {
        const TaskStatus_t *st = &s->status[i];
        memory_task_t *t = &s->tasks[i];
        memcpy(t->name, st->pcTaskName, sizeof(t->name));
        t->name[sizeof(t->name) - 1] = 0;
        t->priority = st->uxCurrentPriority;
        t->free_min = st->usStackHighWaterMark;
        t->external = esp_ptr_external_ram(st->pxStackBase);
        TaskSnapshot_t snap;
        if (st->pxStackBase && vTaskGetSnapshot(st->xHandle, &snap) == pdTRUE) {
            t->stack = (uint32_t)((uintptr_t)snap.pxEndOfStack - (uintptr_t)st->pxStackBase + sizeof(StackType_t));
        }
    }
    s->snapshot_us = esp_timer_get_time() - started;
#if !CONFIG_FREERTOS_UNICORE
    atomic_store(&s->remote_release, true);
#endif
    (void)xTaskResumeAll();
#if !CONFIG_FREERTOS_UNICORE
    while (!atomic_load(&s->remote_done)) vTaskDelay(1);
#endif
    s->result = s->count ? ESP_OK : ESP_ERR_NO_MEM;
#if !CONFIG_FREERTOS_UNICORE
 done:
#endif
    xSemaphoreGive(s->done);
    vTaskDelete(NULL);
}

static cJSON *heap_json(uint32_t caps)
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, caps);
    cJSON *o = cJSON_CreateObject();
    if (!o) return NULL;
    if (!cJSON_AddNumberToObject(o, "total", heap_caps_get_total_size(caps)) ||
        !cJSON_AddNumberToObject(o, "free", info.total_free_bytes) ||
        !cJSON_AddNumberToObject(o, "largest", info.largest_free_block) ||
        !cJSON_AddNumberToObject(o, "minFree", info.minimum_free_bytes) ||
        !cJSON_AddNumberToObject(o, "usedBlocks", info.allocated_blocks) ||
        !cJSON_AddNumberToObject(o, "freeBlocks", info.free_blocks)) {
        cJSON_Delete(o); return NULL;
    }
    return o;
}

static bool add_heap(cJSON *root, const char *name, uint32_t caps)
{
    cJSON *o = heap_json(caps);
    if (!o) return false;
    if (!cJSON_AddItemToObject(root, name, o)) { cJSON_Delete(o); return false; }
    return true;
}
#endif

static esp_err_t memory_get(httpd_req_t *req)
{
    if (!kvm_auth_check(req)) return kvm_auth_challenge(req);
#if CONFIG_FREERTOS_USE_TRACE_FACILITY
    memory_snapshot_t *s = heap_caps_calloc(1, sizeof(*s), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s) return send_json_error(req, "503 Service Unavailable", "out of memory");
    atomic_init(&s->remote_ready, false);
    atomic_init(&s->remote_release, false);
    atomic_init(&s->remote_done, false);
    s->status = heap_caps_calloc(MEMORY_TASK_LIMIT, sizeof(*s->status), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s->tasks = heap_caps_calloc(MEMORY_TASK_LIMIT, sizeof(*s->tasks), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s->done = xSemaphoreCreateBinary();
    bool ok = s->status && s->tasks && s->done;
    if (ok) ok = xTaskCreatePinnedToCore(snapshot_worker, "kvm_mem", 4096, s,
                                        configMAX_PRIORITIES - 2, NULL, 0) == pdPASS;
    if (ok) {
        /* Worker/IPC callbacks must finish before freeing their context. */
        xSemaphoreTake(s->done, portMAX_DELAY);
        ok = s->result == ESP_OK;
    }
    free(s->status);
    if (s->done) vSemaphoreDelete(s->done);
    cJSON *root = ok ? cJSON_CreateObject() : NULL;
    cJSON *tasks = root ? cJSON_AddArrayToObject(root, "tasks") : NULL;
    ok = tasks && add_heap(root, "internal", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) &&
         add_heap(root, "dma", MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) &&
         add_heap(root, "psram", MALLOC_CAP_SPIRAM);
    uint32_t internal_stacks = 0;
    for (UBaseType_t i = 0; ok && i < s->count; i++) {
        const memory_task_t *t = &s->tasks[i];
        if (!t->external) internal_stacks += t->stack;
        cJSON *o = cJSON_CreateObject();
        if (!o) { ok = false; break; }
        ok = cJSON_AddStringToObject(o, "name", t->name) &&
             cJSON_AddNumberToObject(o, "prio", t->priority) &&
             cJSON_AddStringToObject(o, "stackIn", t->external ? "psram" : "internal") &&
             cJSON_AddNumberToObject(o, "stack", t->stack) &&
             cJSON_AddNumberToObject(o, "stackFreeMin", t->free_min);
        if (!ok || !cJSON_AddItemToArray(tasks, o)) { cJSON_Delete(o); ok = false; }
    }
    if (ok) ok = cJSON_AddNumberToObject(root, "internalStacks", internal_stacks) &&
                 cJSON_AddNumberToObject(root, "snapshotUs", s->snapshot_us);
    free(s->tasks); free(s);
    char *json = ok ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    if (!json) return send_json_error(req, "503 Service Unavailable", "memory snapshot unavailable");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    const esp_err_t err = httpd_resp_sendstr(req, json);
    free(json);
    return err;
#else
    return send_json_error(req, "503 Service Unavailable", "task tracing is not compiled");
#endif
}

const httpd_uri_t *memory_api_routes(size_t *count)
{
    static const httpd_uri_t routes[] = {
        {.uri = "/api/v1/system/memory", .method = HTTP_GET, .handler = memory_get},
    };
    *count = sizeof(routes) / sizeof(routes[0]);
    return routes;
}
