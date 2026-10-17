# SPDX-License-Identifier: Apache-2.0
"""Test production snapshot coordination with two host threads and fault injection."""
from pathlib import Path
import subprocess
import tempfile
s=(Path(__file__).resolve().parents[1]/'memory_api.c').read_text()
a=s.index('typedef struct {'); b=s.index('static cJSON *heap_json',a)
source=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CONFIG_FREERTOS_UNICORE 0
#define configMAX_TASK_NAME_LEN 16
#define MEMORY_TASK_LIMIT 128
#define ESP_OK 0
#define ESP_ERR_NO_MEM 1
#define pdTRUE 1
typedef int esp_err_t;
typedef unsigned UBaseType_t;
typedef uint8_t StackType_t;
typedef void *SemaphoreHandle_t;
typedef struct { void *xHandle; const char *pcTaskName; unsigned uxCurrentPriority, usStackHighWaterMark; StackType_t *pxStackBase; } TaskStatus_t;
typedef struct { StackType_t *pxEndOfStack; } TaskSnapshot_t;
static _Thread_local int core;
static atomic_bool paused[2];
static bool fail_ipc, too_many;
static int signalled, deleted;
static pthread_t remote;
static void (*remote_func)(void *);
static void *remote_arg;
static uint8_t stack[128];
static char name[16]="kvm_audio";
static void vTaskSuspendAll(void) { assert(!atomic_exchange(&paused[core],true)); }
static int xTaskResumeAll(void) { assert(atomic_exchange(&paused[core],false)); return 0; }
static void *run_remote(void *unused) { (void)unused; core=1; remote_func(remote_arg); return NULL; }
static int esp_ipc_call(int target, void (*fn)(void *), void *arg) {
    assert(core==0 && target==1 && !atomic_load(&paused[0]));
    if(fail_ipc) return 7;
    remote_func=fn; remote_arg=arg;
    assert(!pthread_create(&remote,NULL,run_remote,NULL)); return ESP_OK;
}
static int64_t esp_timer_get_time(void) { return 100; }
static unsigned uxTaskGetSystemState(TaskStatus_t *st, unsigned cap, void *unused) {
    (void)unused; assert(cap==MEMORY_TASK_LIMIT);
    assert(atomic_load(&paused[0]) && atomic_load(&paused[1]));
    if(too_many) return 0;
    st[0]=(TaskStatus_t){(void *)1,name,5,64,stack}; return 1;
}
static bool esp_ptr_external_ram(void *p) { assert(p==stack); return false; }
static int vTaskGetSnapshot(void *task, TaskSnapshot_t *snap) {
    assert(task==(void *)1 && atomic_load(&paused[0]) && atomic_load(&paused[1]));
    snap->pxEndOfStack=stack+127; return pdTRUE;
}
static void vTaskDelay(int ticks) { assert(ticks==1 && !atomic_load(&paused[0])); sched_yield(); }
static void xSemaphoreGive(void *sem) { assert(sem==(void *)2); assert(!atomic_load(&paused[0]) && !atomic_load(&paused[1])); signalled++; }
static void vTaskDelete(void *task) { assert(!task); deleted++; }
'''+s[a:b]+r'''
int main(void) {
    TaskStatus_t st[MEMORY_TASK_LIMIT]={0}; memory_task_t rows[MEMORY_TASK_LIMIT]={0};
    memory_snapshot_t ctx={.status=st,.tasks=rows,.done=(void *)2};
    snapshot_worker(&ctx); pthread_join(remote,NULL);
    assert(ctx.result==ESP_OK && ctx.count==1 && signalled==1 && deleted==1);
    assert(rows[0].stack==128 && rows[0].free_min==64 && rows[0].priority==5);
    strcpy(name,"deleted"); assert(!strcmp(rows[0].name,"kvm_audio"));
    ctx=(memory_snapshot_t){.status=st,.tasks=rows,.done=(void *)2};
    too_many=true; snapshot_worker(&ctx); pthread_join(remote,NULL);
    assert(ctx.result==ESP_ERR_NO_MEM && ctx.count==0 && signalled==2);
    ctx=(memory_snapshot_t){.status=st,.tasks=rows,.done=(void *)2};
    fail_ipc=true; snapshot_worker(&ctx);
    assert(ctx.result==7 && signalled==3 && deleted==3);
    puts("memory snapshot: two-core coordination, owned names, capacity and IPC failure passed");
}
'''
with tempfile.TemporaryDirectory(prefix='espkvm-memory-test-') as tmp:
 c=Path(tmp)/'test.c'; exe=Path(tmp)/'test'; c.write_text(source)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-pthread','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,timeout=15)
