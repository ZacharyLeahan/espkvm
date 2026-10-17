/*
 * SPDX-FileCopyrightText: 2026 ESP-KVM contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Verify the audio/video container contract with independent packet/box reads.
 * Synthetic AAC bytes exercise muxing, not audio decoding; the optional
 * ffmpeg integration check uses actual encoded media.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ts_mux.h"
#include "ts_to_mp4.h"

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

typedef struct { uint8_t *b; size_t len, pos; } mem_t;
static mem_t ts, mp4;
static void append(mem_t *m, const uint8_t *p, size_t n)
{
    m->b = realloc(m->b, m->len + n);
    if (!m->b) abort();
    memcpy(m->b + m->len, p, n);
    m->len += n;
}
static void packet(void *u, const uint8_t *p) { (void)u; append(&ts, p, TS_PACKET); }
static size_t read_ts(void *io, uint8_t *p, size_t n)
{
    mem_t *m = io;
    if (n > 37) n = 37; /* the I/O callback may split any packet */
    if (n > m->len - m->pos) n = m->len - m->pos;
    memcpy(p, m->b + m->pos, n);
    m->pos += n;
    return n;
}
static bool rewind_ts(void *io) { ((mem_t *)io)->pos = 0; return true; }
static bool write_mp4(void *io, const uint8_t *p, size_t n) { (void)io; append(&mp4, p, n); return true; }
static void reset(void) { free(ts.b); free(mp4.b); ts = (mem_t){0}; mp4 = (mem_t){0}; }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static const uint8_t *child(const uint8_t *p, const char *name, unsigned nth)
{
    if (!p) return NULL;
    const size_t n = be32(p);
    for (size_t i = 8; i + 8 <= n;) {
        const size_t size = be32(p + i);
        if (size < 8 || size > n - i) return NULL;
        if (!memcmp(p + i + 4, name, 4) && nth-- == 0) return p + i;
        i += size;
    }
    return NULL;
}
static const uint8_t *stbl(const uint8_t *trak) { return child(child(child(trak, "mdia", 0), "minf", 0), "stbl", 0); }
static const uint8_t *find_tag(const uint8_t *p, size_t n, const char *tag)
{
    for (size_t i = 4; i + 4 <= n; i++) if (!memcmp(p + i, tag, 4)) return p + i - 4;
    return NULL;
}
static void adts(uint8_t *p, size_t n, uint8_t seed)
{
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)(seed + i);
    const uint8_t h[] = {0xff, 0xf1, 0x4c, (uint8_t)(0x80 | (n >> 11)),
                         (uint8_t)(n >> 3), (uint8_t)((n << 5) | 0x1f), 0xfc};
    memcpy(p, h, sizeof(h));
}
static void video(ts_mux_t *m, uint64_t pts)
{
    const uint8_t au[] = {0,0,0,1,0x67,0x42,0xc0,0x28,0xda,1,
                          0,0,0,1,0x68,0xce,0x3c,0x80,
                          0,0,0,1,0x65,0x11,0x22,0x33};
    ts_mux_frame(m, au, sizeof(au), pts, true);
}
static uint32_t crc(const uint8_t *p, size_t n)
{
    uint32_t c = 0xffffffff;
    while (n--) { c ^= (uint32_t)*p++ << 24; for (int i = 0; i < 8; i++) c = c & 0x80000000 ? c << 1 ^ 0x04c11db7 : c << 1; }
    return c;
}
static size_t payload(const uint8_t *p) { return (p[3] & 0x20) ? 5u + p[4] : 4u; }
static void check_packets(const uint8_t *frames[], const size_t sizes[], const uint64_t pts[], size_t count)
{
    int cc[8192]; for (int i = 0; i < 8192; i++) cc[i] = -1;
    mem_t audio = {0}; size_t got = 0;
    for (size_t off = 0; off < ts.len; off += TS_PACKET) {
        const uint8_t *p = ts.b + off;
        const unsigned pid = ((p[1] & 31) << 8) | p[2];
        const int c = p[3] & 15;
        CHECK(p[0] == 0x47 && (p[3] & 0x10));
        CHECK(cc[pid] < 0 || c == ((cc[pid] + 1) & 15)); cc[pid] = c;
        size_t at = payload(p); CHECK(at <= TS_PACKET);
        if (pid == 0 || pid == 0x1000) {
            const uint8_t *s = p + at + 1 + p[at];
            const size_t n = ((s[1] & 15) << 8) | s[2];
            CHECK(crc(s, n + 3) == 0);
            if (pid == 0x1000) {
                CHECK(n == 23 && s[12] == 0x1b && s[17] == 0x0f);
                CHECK((s[18] & 31) == 1 && s[19] == 1);
                CHECK((s[8] & 31) == 1 && s[9] == 0); /* PCR remains video */
            }
        }
        if (pid != 0x101) continue;
        if (p[1] & 0x40) {
            if (got) { CHECK(audio.len == sizes[got - 1]); CHECK(!memcmp(audio.b, frames[got - 1], audio.len)); }
            audio.len = 0;
            CHECK(got < count);
            if (got >= count) break;
            const uint8_t *h = p + at;
            CHECK(!h[0] && !h[1] && h[2] == 1 && h[3] == 0xc0);
            CHECK(((size_t)h[4] << 8 | h[5]) == sizes[got] + 8);
            CHECK(h[7] == 0x80 && h[8] == 5);
            const uint64_t stamp = ((uint64_t)(h[9] & 14) << 29) | ((uint64_t)h[10] << 22) |
                ((uint64_t)(h[11] & 254) << 14) | ((uint64_t)h[12] << 7) | (h[13] >> 1);
            CHECK(stamp == ((pts[got] + 63000) & 0x1ffffffffull));
            got++; at += 14;
        }
        append(&audio, p + at, TS_PACKET - at);
    }
    CHECK(got == count);
    if (got) { CHECK(audio.len == sizes[got - 1]); CHECK(!memcmp(audio.b, frames[got - 1], audio.len)); }
    free(audio.b);
}
static bool convert(char *err)
{
    mp4_io_t io = {read_ts, rewind_ts, write_mp4, &ts};
    ts.pos = 0;
    return ts_to_mp4(&io, 640, 480, NULL, 0, err, 128);
}
static void test_boundaries(void)
{
    ts_mux_t m; ts_mux_init(&m, packet, NULL); ts_mux_set_audio(&m, true);
    const size_t sizes[] = {8, 169, 170, 171, 354, 355, 356, 8191};
    const uint8_t *frames[8]; uint64_t stamps[8];
    video(&m, 0);
    for (size_t i = 0; i < 8; i++) {
        uint8_t *p = malloc(sizes[i]); adts(p, sizes[i], (uint8_t)i);
        frames[i] = p; stamps[i] = (1ull << 33) - 65000 + i * 1920;
        CHECK(ts_mux_audio_adts(&m, p, sizes[i], stamps[i]));
    }
    check_packets(frames, sizes, stamps, 8);
    const size_t before = ts.len;
    uint8_t bad[20]; adts(bad, sizeof(bad), 0);
    bad[2] = 0x8c; CHECK(!ts_mux_audio_adts(&m, bad, sizeof(bad), 0)); /* not LC */
    adts(bad, sizeof(bad), 0); bad[6] |= 1; CHECK(!ts_mux_audio_adts(&m, bad, sizeof(bad), 0));
    adts(bad, sizeof(bad), 0); bad[2] = 0x50; CHECK(!ts_mux_audio_adts(&m, bad, sizeof(bad), 0)); /* changed rate */
    adts(bad, sizeof(bad), 0); CHECK(!ts_mux_audio_adts(&m, bad, sizeof(bad) - 1, 0));
    ts_mux_set_audio(&m, false); CHECK(!ts_mux_audio_adts(&m, bad, sizeof(bad), 0));
    CHECK(ts.len == before);
    for (size_t i = 0; i < 8; i++) free((void *)frames[i]);
    reset();
}
static void test_movie(void)
{
    ts_mux_t m; ts_mux_init(&m, packet, NULL); ts_mux_set_audio(&m, true);
    uint8_t a[8], b[162], c[507]; adts(a, sizeof(a), 11); adts(b, sizeof(b), 22); adts(c, sizeof(c), 33);
    const uint8_t *frames[] = {a, b, c}; const size_t sizes[] = {sizeof(a), sizeof(b), sizeof(c)};
    const uint64_t stamps[] = {4500, 6420, 10260};
    video(&m, 0); CHECK(ts_mux_audio_adts(&m, a, sizeof(a), stamps[0]));
    video(&m, 3600); CHECK(ts_mux_audio_adts(&m, b, sizeof(b), stamps[1]));
    video(&m, 7200); CHECK(ts_mux_audio_adts(&m, c, sizeof(c), stamps[2]));
    video(&m, 10800);
    check_packets(frames, sizes, stamps, 3);
    char err[128] = ""; CHECK(convert(err)); if (*err) printf("error: %s\n", err);
    const uint8_t *moov = mp4.b + be32(mp4.b);
    const uint8_t *v = child(moov, "trak", 0), *a_track = child(moov, "trak", 1);
    CHECK(v && a_track && !child(moov, "trak", 2));
    const uint8_t *mvhd = child(moov, "mvhd", 0);
    CHECK(mvhd && be32(mvhd + 20) == 90000 && be32(mvhd + 24) == 14400);
    const uint8_t *astbl = stbl(a_track);
    const uint8_t *elst = child(child(a_track, "edts", 0), "elst", 0);
    CHECK(elst && be32(elst + 12) == 2 && be32(elst + 16) == 4500 && be32(elst + 20) == 0xffffffff);
    CHECK(elst && be32(elst + 28) == 7680 && be32(elst + 32) == 0);
    const uint8_t *stts = child(astbl, "stts", 0);
    CHECK(stts && be32(stts + 12) == 3 && be32(stts + 20) == 1920 && be32(stts + 28) == 3840 && be32(stts + 36) == 1920);
    const uint8_t *stsz = child(astbl, "stsz", 0), *stco = child(astbl, "stco", 0);
    CHECK(stsz && stco && be32(stsz + 16) == 3 && be32(stco + 12) == 3);
    if (stsz && stco) for (size_t i = 0; i < 3; i++) {
        CHECK(be32(stsz + 20 + 4 * i) == sizes[i] - 7);
        const size_t off = be32(stco + 16 + 4 * i);
        CHECK(off + sizes[i] - 7 <= mp4.len);
        CHECK(!memcmp(mp4.b + off, frames[i] + 7, sizes[i] - 7));
    }
    const uint8_t *vco = child(stbl(v), "stco", 0);
    CHECK(vco && be32(vco + 12) == 4);
    if (vco) for (size_t i = 0; i < 4; i++) { const size_t off = be32(vco + 16 + 4 * i); CHECK(be32(mp4.b + off) == 4 && mp4.b[off + 4] == 0x65); }
    const uint8_t *esds = find_tag(a_track, be32(a_track), "esds");
    CHECK(esds && esds[32] == 5 && esds[33] == 2 && esds[34] == 0x11 && esds[35] == 0x90);
    reset();
}
static void test_rejects(void)
{
    for (int scenario = 0; scenario < 10; scenario++) {
        ts_mux_t m; ts_mux_init(&m, packet, NULL); ts_mux_set_audio(&m, true);
        uint8_t a[507]; adts(a, sizeof(a), 11);
        video(&m, 0); CHECK(ts_mux_audio_adts(&m, a, sizeof(a), 4500));
        if (scenario == 4) CHECK(ts_mux_audio_adts(&m, a, sizeof(a), 6420));
        size_t audio_count = 0;
        for (size_t i = 0; i < ts.len; i += TS_PACKET) {
            uint8_t *p = ts.b + i;
            if ((((p[1] & 31) << 8) | p[2]) != 0x101 || !(p[1] & 0x40)) continue;
            audio_count++;
            uint8_t *h = p + payload(p);
            if (scenario == 0) h[14 + 2] = 0x8c; /* wrong object type */
            if (scenario == 1) h[14 + 4]--; /* wrong frame length */
            if (scenario == 2) h[5]--; /* wrong PES length */
            if (scenario == 4 && audio_count == 2) h[14 + 2] = 0x50; /* format change */
            if (scenario == 5) h[2] = 2; /* invalid PES start */
            if (scenario == 6) h[3] = 0xe0; /* not an audio PES */
            if (scenario == 7) h[7] = 0; /* missing PTS */
            if (scenario == 8) h[8] = 255; /* invalid header extent */
            if (scenario == 9) { p[3] |= 0x20; p[4] = 183; } /* missing payload */
        }
        if (scenario == 3) ts.len -= TS_PACKET; /* interrupted ADTS frame */
        char err[128] = ""; CHECK(!convert(err)); CHECK(*err); CHECK(mp4.len == 0);
        reset();
    }
}
static void test_padded_join(void)
{
    uint8_t a[20]; adts(a, sizeof(a), 42);
    for (int segment = 0; segment < 2; segment++) {
        ts_mux_t m; ts_mux_init(&m, packet, NULL); ts_mux_set_audio(&m, true);
        video(&m, 0); CHECK(ts_mux_audio_adts(&m, a, sizeof(a), 0));
        video(&m, 3600); CHECK(ts_mux_audio_adts(&m, a, sizeof(a), 6000));
        /* This AAC frame ends at 7920, beyond the 7200 video boundary. */
    }
    char err[128] = ""; CHECK(convert(err)); if (*err) printf("error: %s\n", err);
    const uint8_t *moov = mp4.b + be32(mp4.b);
    const uint8_t *v = child(moov, "trak", 0), *a_track = child(moov, "trak", 1);
    const uint8_t *mdhd = child(child(v, "mdia", 0), "mdhd", 0);
    CHECK(mdhd && be32(mdhd + 24) == 14400); /* no padding accumulated in video */
    const uint8_t *stts = child(stbl(a_track), "stts", 0);
    CHECK(stts && be32(stts + 12) == 4);
    CHECK(stts && be32(stts + 20) == 6000 && be32(stts + 28) == 1200);
    CHECK(stts && be32(stts + 36) == 6000 && be32(stts + 44) == 1920);
    reset();
}
int main(void)
{
    test_boundaries(); test_movie(); test_padded_join(); test_rejects();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    puts("ts_audio: ADTS/PES boundaries, PMT CRC, continuity, PTS wrap, MP4 offsets/gaps, malformed input - all as expected");
    return 0;
}
