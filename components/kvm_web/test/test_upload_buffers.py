# SPDX-License-Identifier: Apache-2.0
"""Compile the production upload allocator with fault-injected heap/queue APIs."""
from pathlib import Path
import re
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'http_server.c').read_text()
start = source.index('static int upload_buffers_alloc(')
end = source.index('\nstatic void upload_writer_task', start)
defines = '\n'.join(re.findall(r'^#define UPLOAD_(?:CHUNK|BUFS|CHUNK_MIN) .+$', source, re.M))
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define pdTRUE 1
#define MALLOC_CAP_SPIRAM 1
'''+defines+r'''
typedef struct { char *data; size_t len; } upload_buf_t;
typedef struct { upload_buf_t b[4]; int n; } queue_t;
typedef struct { queue_t *empty; } upload_writer_t;
static size_t largest, budget, used;
static int live, attempts, reject_send;
typedef struct { void *p; size_t n; } allocation_t;
static allocation_t allocations[4];
static void *heap_caps_aligned_alloc(size_t align, size_t n, int caps) {
    assert(align == 64 && caps == MALLOC_CAP_SPIRAM);
    attempts++;
    if (n > largest || n > budget - used) return NULL;
    void *p = malloc(n); assert(p);
    for (int i = 0; i < 4; i++) if (!allocations[i].p) {
        allocations[i] = (allocation_t){p, n}; used += n; live++; return p;
    }
    abort();
}
static void checked_free(void *p) {
    for (int i = 0; i < 4; i++) if (allocations[i].p == p) {
        used -= allocations[i].n; allocations[i].p = NULL; live--; free(p); return;
    }
    abort(); /* double free / unowned allocation */
}
static int xQueueSend(queue_t *q, const upload_buf_t *b, int wait) {
    assert(wait == 0);
    if (reject_send || q->n == 4) return 0;
    q->b[q->n++] = *b; return pdTRUE;
}
static int xQueueReceive(queue_t *q, upload_buf_t *b, int wait) {
    assert(wait == 0);
    if (!q->n) return 0;
    *b = q->b[--q->n]; return pdTRUE;
}
#define free checked_free
'''+source[start:end]+r'''
static void run(size_t max_block, size_t total, int count, size_t chunk_want, bool queue_fail) {
    largest = max_block; budget = total; attempts = 0; reject_send = queue_fail;
    assert(live == 0 && used == 0);
    queue_t q = {0}; upload_writer_t w = {.empty = &q}; size_t chunk = 0;
    assert(upload_buffers_alloc(&w, &chunk) == count);
    assert(q.n == count && live == count && chunk == chunk_want);
    assert(attempts <= 20); /* finite under total memory exhaustion */
    assert(used == (size_t)count * chunk);
    upload_buf_t b;
    /* The worker takes this same cleanup path if writer creation fails. */
    while (xQueueReceive(&q, &b, 0)) {
        assert(b.len == 0);
        memset(b.data, 0x5a, chunk); /* all buffers support the chosen capacity */
        free(b.data);
    }
    assert(live == 0 && used == 0);
}
int main(void) {
    run(262144, 1048576, 4, 262144, false);
    run(65536, 1048576, 4, 65536, false); /* fragmented */
    run(262144, 524288, 2, 262144, false); /* fewer large buffers */
    run(262144, 262144, 2, 131072, false); /* free first attempt before retry */
    run(32768, 32768, 1, 32768, false); /* one minimum-size buffer still usable */
    run(32767, 1048576, 0, 32768, false);
    run(262144, 0, 0, 32768, false);
    run(262144, 1048576, 0, 262144, true); /* queue rejection does not leak */
    puts("upload allocator: eight fault scenarios passed");
}
'''
with tempfile.TemporaryDirectory(prefix='espkvm-upload-test-') as tmp:
    c = Path(tmp) / 'test.c'
    exe = Path(tmp) / 'test'
    c.write_text(harness)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
