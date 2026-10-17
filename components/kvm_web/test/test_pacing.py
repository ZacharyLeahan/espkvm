# SPDX-License-Identifier: Apache-2.0
"""Exercise production WebSocket fragmentation and whole-message locking."""
from pathlib import Path
import subprocess
import tempfile
source = (Path(__file__).resolve().parents[1] / 'http_server.c').read_text()
a = source.index('static esp_err_t ws_send_paced(')
b = source.index('static void ws_send_binary(', a)
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef int esp_err_t;
typedef void *httpd_handle_t;
typedef struct { bool final, fragmented; int type; uint8_t *payload; size_t len; } httpd_ws_frame_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE -1
#define HTTPD_WS_TYPE_CONTINUE 0
static int locked, calls, yields, fail_call;
static size_t received, expected;
static uint8_t data[140000];
static int tx_lock(int fd) { assert(fd==1 && !locked); locked=1; return 0; }
static void tx_unlock(int slot) { assert(slot==0 && locked); locked=0; }
static void vTaskDelay(int ticks) { assert(ticks==1 && locked); yields++; }
static esp_err_t ws_send_frame(httpd_handle_t server, int fd, httpd_ws_frame_t *f) {
    assert(server && fd==1 && f->payload==data+received);
    calls++;
    if (calls==fail_call) return -2;
    if (expected>1024) {
        assert(locked && f->fragmented && f->len<=1024);
        assert(f->type==(received ? HTTPD_WS_TYPE_CONTINUE : 2));
        assert(f->final==(received+f->len==expected));
    }
    received+=f->len;
    return ESP_OK;
}
'''+source[a:b]+r'''
int main(void) {
    const size_t sizes[]={1,1024,1025,16384,16385,139143};
    for (unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++) {
        expected=sizes[i];received=calls=yields=fail_call=0;
        httpd_ws_frame_t f={.final=true,.type=2,.payload=data,.len=expected};
        assert(ws_send_paced((void *)1,1,&f)==ESP_OK);
        assert(!locked && received==expected);
        assert(yields==(int)((expected-1)/4096));
    }
    expected=139143;received=calls=yields=0;fail_call=3;
    httpd_ws_frame_t f={.final=true,.type=2,.payload=data,.len=expected};
    assert(ws_send_paced((void *)1,1,&f)==-2 && !locked && calls==3 && received==2048);
    puts("WebSocket pacing: fragments, final flags, bounded yields and failure unlock passed");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp) / 'test.c'
    binary = Path(tmp) / 'test'
    c.write_text(harness)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
