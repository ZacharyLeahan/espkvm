/*
 * SPDX-FileCopyrightText: 2026 ESP-KVM contributors
 * SPDX-License-Identifier: Apache-2.0
 * Host driver for test_audio_roundtrip.py's encoded media fixtures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "ts_mux.h"
#include "ts_to_mp4.h"

typedef struct { FILE *ts, *mp4; bool failed; } files_t;
static void packet(void *user, const uint8_t *p)
{
    files_t *f = user;
    if (fwrite(p, 1, TS_PACKET, f->ts) != TS_PACKET) f->failed = true;
}
static size_t read_ts(void *user, uint8_t *p, size_t n) { return fread(p, 1, n, ((files_t *)user)->ts); }
static bool rewind_ts(void *user) { return fseek(((files_t *)user)->ts, 0, SEEK_SET) == 0; }
static bool write_mp4(void *user, const uint8_t *p, size_t n) { return fwrite(p, 1, n, ((files_t *)user)->mp4) == n; }
int main(int argc, char **argv)
{
    if (argc != 4) return 2;
    const bool remux_only = strcmp(argv[1], "--remux") == 0;
    FILE *input = remux_only ? NULL : fopen(argv[1], "rb");
    files_t f = {.ts = fopen(argv[2], remux_only ? "rb" : "w+b"), .mp4 = fopen(argv[3], "wb")};
    if ((!remux_only && !input) || !f.ts || !f.mp4) return 2;
    ts_mux_t mux; ts_mux_init(&mux, packet, &f); ts_mux_set_audio(&mux, true);
    uint8_t hdr[13];
    while (input && fread(hdr, 1, sizeof(hdr), input) == sizeof(hdr)) {
        uint32_t size = 0; uint64_t pts = 0;
        for (int i = 0; i < 4; i++) size |= (uint32_t)hdr[1 + i] << (i * 8);
        for (int i = 0; i < 8; i++) pts |= (uint64_t)hdr[5 + i] << (i * 8);
        uint8_t *data = malloc(size);
        if (!data || fread(data, 1, size, input) != size) return 2;
        if (hdr[0] == 1) {
            if (!ts_mux_audio_adts(&mux, data, size, pts)) return 3;
        } else {
            ts_mux_frame(&mux, data, size, pts, true);
        }
        free(data);
    }
    if (input) fclose(input);
    if (f.failed || !rewind_ts(&f)) return 3;
    const mp4_io_t io = {read_ts, rewind_ts, write_mp4, &f};
    char err[128] = "";
    if (!ts_to_mp4(&io, 320, 240, NULL, 0, err, sizeof(err))) { fprintf(stderr, "%s\n", err); return 4; }
    fclose(f.ts); fclose(f.mp4);
    return 0;
}
