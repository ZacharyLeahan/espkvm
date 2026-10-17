#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 ESP-KVM contributors
# SPDX-License-Identifier: Apache-2.0
"""Run the real timing poll and I2C helpers against fault-injected host I2C."""
from pathlib import Path
import re
import subprocess
import tempfile

component = Path(__file__).resolve().parents[1]
source = (component / 'tc358743.c').read_text()

def function(name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;]*?\n\{', source, re.M)
    assert match, name
    end = source.index('\n}', match.end()) + 2
    return source[match.start():end]

names = ('i2c_note_result', 'i2c_write_reg', 'i2c_read_reg', 'wr8', 'rd8', 'rd16',
         'tc358743_read_hact_vact_htotal', 'tc358743_get_timings')
defines = '\n'.join(re.findall(r'^#define .*?(?<!\\)$', source, re.M))
struct = source[source.index('struct tc358743 {'):source.index('\n};', source.index('struct tc358743 {'))+3]
prelude = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tc358743.h"
#define CONFIG_KVM_ENABLE_AUDIO 1
#define CONFIG_KVM_TC358743_ADV_DEBUG 1
typedef unsigned TickType_t;
static unsigned ticks;
static TickType_t xTaskGetTickCount(void) { return ticks += 6000; }
#define pdMS_TO_TICKS(n) (n)
static const char *TAG = "test";
static void quiet_log(const char *format, ...) { (void)format; }
#define ESP_LOGI(tag, ...) do { (void)(tag); quiet_log(__VA_ARGS__); } while (0)
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI
#define ESP_RETURN_ON_FALSE(cond, err, ...) do { if (!(cond)) return err; } while (0)
static const char *esp_err_to_name(esp_err_t e) { (void)e; return "injected"; }
static unsigned calls, fail_at, writes;
static esp_err_t fault = ESP_ERR_TIMEOUT;
static unsigned char regs[65536];
static esp_err_t i2c_master_transmit_receive(void *dev, const void *a, size_t an, void *v, size_t n, int timeout)
{
    (void)dev;
    assert(an == 2 && timeout == 200);
    if (++calls == fail_at) return fault;
    const unsigned char *addr = a;
    memcpy(v, regs + ((unsigned)addr[0] << 8) + addr[1], n);
    return ESP_OK;
}
static esp_err_t i2c_master_transmit(void *dev, const void *p, size_t n, int timeout)
{
    (void)dev;
    assert(n >= 3 && timeout == 200);
    if (++calls == fail_at) return fault;
    const unsigned char *data = p;
    memcpy(regs + ((unsigned)data[0] << 8) + data[1], data + 2, n - 2);
    writes++;
    return ESP_OK;
}
'''
tests = r'''
static void reset(void)
{
    memset(regs, 0, sizeof regs);
    regs[HACT0] = 0; regs[HACT1] = 5; /* 1280 */
    regs[VACT0] = 0xd0; regs[VACT1] = 2; /* 720 */
    regs[SYS_STATUS] = 0x93;
    regs[FV_CNT_LO] = 167;
    calls = writes = fail_at = 0;
}
int main(void)
{
    tc358743_t d = {0};
    tc358743_timings_t out, zero = {0};
    reset();
    assert(tc358743_get_timings(&d, &out) == ESP_OK);
    assert(out.hact == 1280 && out.vact == 720 && out.hz == 60);
    assert(out.sync && out.hdmi_mode && out.ddc5v && out.tmds);
    assert(regs[ANA_CTL] == 0x31 && writes == 1 && !d.timing_poll);
    const unsigned count = calls;
    /* Fail every real read/write position, including the audio PLL write.
     * Each failed poll issues no further transfers and the next poll recovers. */
    for (unsigned kind = 0; kind < 2; kind++) {
        fault = kind ? ESP_FAIL : ESP_ERR_TIMEOUT;
        for (unsigned n = 1; n <= count; n++) {
            reset(); fail_at = n;
            assert(tc358743_get_timings(&d, &out) == fault);
            assert(calls == n && !d.timing_poll);
            assert(memcmp(&out, &zero, sizeof out) == 0);
            calls = fail_at = 0;
            assert(tc358743_get_timings(&d, &out) == ESP_OK);
            assert(out.sync && out.hact == 1280 && regs[ANA_CTL] == 0x31);
        }
    }
    reset(); regs[SYS_STATUS] = 0; /* A real signal loss is not a bus error. */
    assert(tc358743_get_timings(&d, &out) == ESP_OK && !out.sync);
    assert(writes == 0);
    assert(tc358743_get_timings(NULL, &out) == ESP_ERR_INVALID_ARG);
    assert(tc358743_get_timings(&d, NULL) == ESP_ERR_INVALID_ARG);
    printf("TC358743: %u injected failures/recoveries, HDMI PLL and real signal loss passed\n", count * 2);
}
'''
with tempfile.TemporaryDirectory(prefix='espkvm-bridge-test-') as temp:
    root = Path(temp)
    (root / 'driver').mkdir()
    (root / 'driver/i2c_master.h').write_text('typedef void *i2c_master_bus_handle_t;\ntypedef void *i2c_master_dev_handle_t;\n')
    (root / 'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_TIMEOUT 1\n#define ESP_ERR_INVALID_ARG 2\n#define ESP_ERR_INVALID_SIZE 3\n#define ESP_ERR_NOT_SUPPORTED 4\n')
    test = root / 'test.c'
    test.write_text(prelude + defines + '\n' + struct + '\n' + '\n'.join(map(function, names)) + tests)
    exe = root / 'test'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I', str(root), '-I', str(component / 'include'),
                    '-I', str(component.parent / 'kvm_bridge/include'), str(test), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=20)
