# SPDX-License-Identifier: Apache-2.0
"""Exercise the production session locking and HTTP/TLS write loops with host mocks."""
from pathlib import Path
import subprocess
import tempfile
s = (Path(__file__).resolve().parents[1] / 'http_server.c').read_text()
a = s.index('static int tx_lock('); b = s.index('/* The control WebSockets', a)
c = s.index('static int locked_ssl_send('); d = s.index('static esp_err_t ws_send_frame', c)
harness = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#define TX_MAX_SOCKETS 16
#define portMAX_DELAY -1
#define HTTPD_SOCK_ERR_FAIL -1
#define ESP_TLS_ERR_SSL_WANT_WRITE -2
#define ESP_TLS_ERR_SSL_WANT_READ -3
#define pdTRUE 1
typedef pthread_mutex_t *SemaphoreHandle_t;
typedef void *httpd_handle_t;
typedef struct { void *tls; } https_transport_head_t;
static SemaphoreHandle_t s_tx_mu, s_tx_slot_mu[TX_MAX_SOCKETS];
static int s_tx_fds[TX_MAX_SOCKETS];
static uint32_t s_tx_gen[TX_MAX_SOCKETS];
static pthread_mutex_t table, slots[TX_MAX_SOCKETS];
static bool recycle_slot;
static int xSemaphoreTakeRecursive(SemaphoreHandle_t m, int ticks) {
    (void)ticks;
    if (m == &slots[0] && recycle_slot) {
        recycle_slot = false;
        pthread_mutex_lock(&table); s_tx_gen[0]++; pthread_mutex_unlock(&table);
    }
    return pthread_mutex_lock(m) == 0;
}
static void xSemaphoreGiveRecursive(SemaphoreHandle_t m) { assert(!pthread_mutex_unlock(m)); }
static int64_t now;
static int writes, yields, shut, context_reads, count, next;
static ssize_t results[8];
static int64_t timeout_us;
static https_transport_head_t ctx = {(void *)1};
static const char *expected;
static size_t offset;
static void *httpd_sess_get_transport_ctx(httpd_handle_t hd, int fd) {
    (void)hd; assert(fd == 10); context_reads++; return &ctx;
}
static int64_t esp_timer_get_time(void) { return now; }
static void vTaskDelay(int ticks) { assert(ticks == 1); yields++; now += 10000; }
static int mock_getsockopt(int fd, int level, int option, void *value, socklen_t *len) {
    (void)fd; (void)level; (void)option; assert(*len == sizeof(struct timeval));
    struct timeval *tv = value; tv->tv_sec = timeout_us / 1000000; tv->tv_usec = timeout_us % 1000000; return 0;
}
static int mock_shutdown(int fd, int how) { assert(fd == 10 && how == SHUT_RDWR); shut++; return 0; }
static ssize_t esp_tls_conn_write(void *tls, const char *buf, size_t len) {
    assert(tls == (void *)1 && buf == expected + offset && len == 6 - offset); writes++;
    ssize_t r = next < count ? results[next++] : ESP_TLS_ERR_SSL_WANT_WRITE;
    if (r > 0) offset += (size_t)r;
    return r;
}
static int send_errno;
static int64_t send_elapsed;
static ssize_t mock_send(int fd, const char *buf, size_t len, int flags) {
    assert(fd == 10 && flags == 0 && buf == expected + offset && len == 6 - offset);
    writes++; now += send_elapsed;
    ssize_t r = next < count ? results[next++] : -1;
    if (r > 0) offset += (size_t)r;
    errno = send_errno;
    return r;
}
#define send mock_send
#define getsockopt mock_getsockopt
#define shutdown mock_shutdown
'''+s[a:b]+s[c:d]+r'''
static void reset(void) {
    now = writes = yields = shut = context_reads = count = next = offset = 0;
    send_errno = EAGAIN; send_elapsed = 0;
    timeout_us = 100000; expected = "abcdef"; ctx.tls = (void *)1;
}
static void *other_session(void *arg) {
    (void)arg;
    int slot = tx_lock(11); assert(slot == 1); tx_unlock(slot); return NULL;
}
int main(void) {
    pthread_mutexattr_t attr; pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&table, &attr); s_tx_mu = &table;
    for (int i = 0; i < TX_MAX_SOCKETS; i++) {
        pthread_mutex_init(&slots[i], &attr); s_tx_slot_mu[i] = &slots[i]; s_tx_fds[i] = -1;
    }
    s_tx_fds[0] = 10; s_tx_fds[1] = 11;
    assert(tx_lock(-1) == -1 && tx_lock(99) == -1);
    int slot = tx_lock(10); assert(slot == 0);
    assert(tx_lock(10) == 0); tx_unlock(0); /* nested frame -> TLS lock */
    pthread_t thread; assert(!pthread_create(&thread, NULL, other_session, NULL));
    pthread_join(thread, NULL); /* another session completes while this lock is held */
    tx_unlock(slot);
    reset(); results[0] = 2; results[1] = 0; results[2] = -3; results[3] = 4; count = 4;
    assert(locked_ssl_send(NULL,10,expected,6,0) == 6);
    assert(writes == 4 && yields == 2 && shut == 0 && offset == 6);
    reset();
    assert(locked_ssl_send(NULL,10,expected,6,0) == -1);
    assert(now == 100000 && writes == 10 && yields == 10 && shut == 1);
    reset(); timeout_us = 10000000;
    assert(locked_ssl_send(NULL,10,expected,6,0) == -1);
    assert(now == 5000000 && yields == 500 && shut == 1);
    reset(); results[0] = 2; results[1] = -99; count = 2;
    assert(locked_ssl_send(NULL,10,expected,6,0) == -1 && shut == 1);
    reset(); ctx.tls = NULL;
    assert(locked_ssl_send(NULL,10,expected,6,0) == -1 && writes == 0);
    reset(); s_tx_fds[0] = -1;
    assert(locked_ssl_send(NULL,10,expected,6,0) == -1 && context_reads == 0);
    s_tx_fds[0] = 10;
    reset(); results[0] = 2; results[1] = 4; count = 2;
    assert(locked_http_send(NULL,10,expected,6,0) == 6);
    assert(writes == 2 && offset == 6 && shut == 0);
    reset(); results[0] = 2; count = 1;
    assert(locked_http_send(NULL,10,expected,6,0) == -1);
    assert(writes == 2 && offset == 2 && shut == 1 && yields == 0);
    reset(); results[0] = 0; count = 1;
    assert(locked_http_send(NULL,10,expected,6,0) == -1 && shut == 1);
    reset(); send_errno = EINTR; results[0] = -1; results[1] = 6; count = 2;
    assert(locked_http_send(NULL,10,expected,6,0) == 6 && yields == 1);
    reset(); send_errno = EPIPE;
    assert(locked_http_send(NULL,10,expected,6,0) == -1 && writes == 1 && shut == 1);
    reset(); send_elapsed = 100000; results[0] = 2; results[1] = 4; count = 2;
    assert(locked_http_send(NULL,10,expected,6,0) == -1 && writes == 1 && shut == 1);
    reset(); s_tx_fds[0] = -1;
    assert(locked_http_send(NULL,10,expected,6,0) == -1 && writes == 0);
    s_tx_fds[0] = 10;
    recycle_slot = true;
    assert(tx_lock(10) == -1); /* same descriptor reused after table snapshot */
    assert(tx_lock(10) == 0); tx_unlock(0); /* new sender can use the replacement */
    puts("session TX: independent/recursive locks, partial writes, yield, PCM timeout and closed TLS passed");
    for (int i = 0; i < TX_MAX_SOCKETS; i++) pthread_mutex_destroy(&slots[i]);
    pthread_mutex_destroy(&table); pthread_mutexattr_destroy(&attr);
}
'''
with tempfile.TemporaryDirectory(prefix='espkvm-tx-test-') as tmp:
    c=Path(tmp)/'test.c'; exe=Path(tmp)/'test'; c.write_text(harness)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-pthread','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=15)
