#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check read-only diagnostic ACK parsing against malformed and unrelated frames."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'http_server.c').read_text()
start = source.index('static atomic_uint probe_peer_port')
end = source.index('esp_err_t __real_esp_netif_receive', start)
harness = r'''
#include <arpa/inet.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
static int64_t esp_timer_get_time(void) { return 123456000; }
''' + source[start:end] + r'''
static void put16(uint8_t *p, uint16_t n) { p[0] = n >> 8; p[1] = n; }
static uint16_t check(uint32_t n) {
    while (n >> 16) n = (n & 65535) + (n >> 16);
    return (uint16_t)~n;
}
int main(void) {
    uint8_t frame[60] = {0}, saved[60];
    uint8_t *ip = frame + 14, *tcp = ip + 20;
    put16(frame + 12, 0x0800); ip[0] = 0x45; ip[8] = 64; ip[9] = 6;
    put16(ip + 2, 40);
    uint32_t src = htonl(0xc0a801d3), dst = htonl(0xc0a801a9);
    memcpy(ip + 12, &src, 4); memcpy(ip + 16, &dst, 4);
    atomic_store(&probe_peer_addr, src); atomic_store(&probe_local_addr, dst);
    atomic_store(&probe_peer_port, 60000);
    put16(tcp, 60000); put16(tcp + 2, 80); tcp[12] = 0x50; tcp[13] = 0x10;
    tcp[8] = 0xfe; tcp[9] = 0xdc; tcp[10] = 0xba; tcp[11] = 0x98;
    put16(ip + 10, check(probe_sum(ip, 20)));
    put16(tcp + 16, check(probe_sum(ip + 12, 8) + 6 + 20 + probe_sum(tcp, 20)));
    memcpy(saved, frame, sizeof(frame));
    probe_rx_frame(frame, sizeof(frame));
    assert(atomic_load(&probe_rx_count) == 1 && atomic_load(&probe_rx_ack) == 0xfedcba98);
    assert(atomic_load(&probe_rx_tick) == 123456 && atomic_load(&probe_rx_bad) == 0);
    assert(memcmp(frame, saved, sizeof(frame)) == 0);
    for (size_t n = 0; n < 54; n++) probe_rx_frame(frame, n);
    probe_rx_frame(NULL, 60);
    assert(atomic_load(&probe_rx_count) == 1);
    tcp[8] ^= 1; probe_rx_frame(frame, 54);
    assert(atomic_load(&probe_rx_bad) == 1 && atomic_load(&probe_rx_count) == 1);
    memcpy(frame, saved, sizeof(frame));
    atomic_store(&probe_peer_port, 60001); probe_rx_frame(frame, 60);
    atomic_store(&probe_peer_port, 60000);
    put16(ip + 2, 65535); probe_rx_frame(frame, 60);
    memcpy(frame, saved, sizeof(frame)); ip[0] = 0x4f; probe_rx_frame(frame, 60);
    memcpy(frame, saved, sizeof(frame)); put16(ip + 6, 0x2000); probe_rx_frame(frame, 60);
    memcpy(frame, saved, sizeof(frame)); put16(frame + 12, 0x8100); probe_rx_frame(frame, 60);
    assert(atomic_load(&probe_rx_count) == 1 && atomic_load(&probe_rx_bad) == 1);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    src = Path(directory) / 'probe.c'
    exe = Path(directory) / 'probe'
    src.write_text(harness)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('RX probe: valid ACK, checksum, tuple, truncation, fragments and read-only parsing passed')
