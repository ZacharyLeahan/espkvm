/*
 * SPDX-FileCopyrightText: 2026 ESP-KVM contributors
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ts_to_mp4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TS 188
#define PID_VIDEO 0x0100
#define PID_AUDIO 0x0101
#define TIMESCALE 90000u

/* ---- reading the transport stream --------------------------------------------- */

typedef struct {
    uint8_t *b;
    size_t len;
    size_t cap;
    int64_t pts;
    bool audio;
    size_t expected; /* finite audio PES payload length */
} au_buf_t;

typedef struct {
    const mp4_io_t *io;
    uint8_t pkt[TS];
    au_buf_t cur;   /* the access unit being gathered */
    au_buf_t ready; /* the last one handed out */
    bool open;
    bool eof;
} ts_reader_t;

static bool au_append(au_buf_t *a, const uint8_t *p, size_t n)
{
    if (a->len + n > a->cap) {
        /* Small steps from a small start: on a board with a megabyte of PSRAM
         * left, a doubled buffer is the allocation that fails. */
        size_t cap = a->cap ? a->cap + a->cap / 2 : 64 * 1024;
        while (cap < a->len + n) {
            cap *= 2;
        }
        uint8_t *grown = realloc(a->b, cap);
        if (!grown) {
            return false;
        }
        a->b = grown;
        a->cap = cap;
    }
    memcpy(a->b + a->len, p, n);
    a->len += n;
    return true;
}

/* The gathered access unit becomes the ready one; the old ready buffer is reused. */
static void hand_out(ts_reader_t *r)
{
    const au_buf_t t = r->ready;
    r->ready = r->cur;
    r->cur = t;
    r->cur.len = 0;
}

static void reader_free(ts_reader_t *r)
{
    free(r->cur.b);
    free(r->ready.b);
}

static bool read_packet(ts_reader_t *r)
{
    size_t have = 0;
    while (have < TS) {
        const size_t n = r->io->read(r->io->io, r->pkt + have, TS - have);
        if (!n) {
            break;
        }
        have += n;
    }
    return have == TS && r->pkt[0] == 0x47;
}

/* 1 = access unit, 0 = end, -1 = allocation failure, -2 = malformed audio PES. */
static int next_au(ts_reader_t *r)
{
    for (;;) {
        if (r->eof || !read_packet(r)) {
            r->eof = true;
            if (r->open) {
                if (r->cur.audio && r->cur.len != r->cur.expected) return -2;
                r->open = false;
                hand_out(r);
                return 1;
            }
            return 0;
        }
        const uint8_t *p = r->pkt;
        const int pid = ((p[1] & 0x1f) << 8) | p[2];
        if ((pid != PID_VIDEO && pid != PID_AUDIO) || !(p[3] & 0x10)) {
            continue;
        }
        size_t at = 4;
        if (p[3] & 0x20) {
            at = 5 + p[4];
            if (at >= TS) {
                if (pid == PID_AUDIO) return -2;
                continue;
            }
        }
        if (!(p[1] & 0x40)) {
            if (r->open && r->cur.audio != (pid == PID_AUDIO)) return -2;
            if (r->open && r->cur.audio && TS - at > r->cur.expected - r->cur.len) return -2;
            if (r->open && !au_append(&r->cur, p + at, TS - at)) {
                return -1;
            }
            continue;
        }
        /* A PES packet starts: whatever was gathered before it is complete. */
        const uint8_t *h = p + at;
        if (TS - at < 14 || h[0] || h[1] || h[2] != 1) {
            if (pid == PID_AUDIO) return -2;
            continue;
        }
        const bool audio = pid == PID_AUDIO;
        const size_t pes_len = ((size_t)h[4] << 8) | h[5];
        if (audio && (h[3] != 0xc0 || (h[7] & 0xc0) != 0x80 || h[8] != 5 ||
                      pes_len < 8 || pes_len > 8191 + 8 || (h[9] & 0xf1) != 0x21 ||
                      !(h[11] & 1) || !(h[13] & 1))) return -2;
        int64_t pts = 0;
        if (h[7] & 0x80) {
            pts = ((int64_t)(h[9] & 0x0e) << 29) | ((int64_t)h[10] << 22) |
                  ((int64_t)(h[11] & 0xfe) << 14) | ((int64_t)h[12] << 7) | (h[13] >> 1);
        }
        const size_t payload = at + 9 + h[8];
        const bool had = r->open;
        if (had) {
            if (r->cur.audio && r->cur.len != r->cur.expected) return -2;
            hand_out(r);
        }
        r->open = true;
        r->cur.pts = pts;
        r->cur.audio = audio;
        r->cur.expected = audio ? pes_len - 8 : 0;
        if (audio && TS - payload > r->cur.expected) return -2;
        if (payload < TS && !au_append(&r->cur, p + payload, TS - payload)) {
            return -1;
        }
        if (had) {
            return 1;
        }
    }
}

/* ---- H.264 --------------------------------------------------------------------- */

typedef void (*nal_fn)(void *user, const uint8_t *nal, size_t len);

/* Call @p fn for each NAL unit in an Annex-B access unit. */
static void each_nal(const uint8_t *b, size_t n, nal_fn fn, void *user)
{
    size_t i = 0, start = SIZE_MAX;
    while (i + 3 <= n) {
        if (b[i] == 0 && b[i + 1] == 0 && b[i + 2] == 1) {
            if (start != SIZE_MAX) {
                size_t end = i;
                if (end > start && b[end - 1] == 0) {
                    end--; /* the leading zero of a four-byte start code */
                }
                fn(user, b + start, end - start);
            }
            i += 3;
            start = i;
        } else {
            i++;
        }
    }
    if (start != SIZE_MAX && start < n) {
        fn(user, b + start, n - start);
    }
}

typedef struct {
    size_t size; /* the sample, length-prefixed */
    bool idr;
    uint8_t *sps, *pps;
    size_t sps_len, pps_len;
    const mp4_io_t *io; /* writing pass only */
    bool ok;
} nal_scan_t;

static bool keep_nal(uint8_t type)
{
    /* The access unit delimiter and parameter sets do not go in the samples:
     * the parameter sets live in the sample description instead. */
    return type != 9 && type != 7 && type != 8;
}

static void scan_nal(void *user, const uint8_t *nal, size_t len)
{
    nal_scan_t *s = user;
    if (!len) {
        return;
    }
    const uint8_t type = nal[0] & 0x1f;
    if (type == 5) {
        s->idr = true;
    }
    if (type == 7 && !s->sps && len >= 4) {
        s->sps = malloc(len);
        if (s->sps) {
            memcpy(s->sps, nal, len);
            s->sps_len = len;
        }
    }
    if (type == 8 && !s->pps) {
        s->pps = malloc(len);
        if (s->pps) {
            memcpy(s->pps, nal, len);
            s->pps_len = len;
        }
    }
    if (keep_nal(type)) {
        s->size += 4 + len;
    }
}

static void write_nal(void *user, const uint8_t *nal, size_t len)
{
    nal_scan_t *s = user;
    if (!len || !s->ok || !keep_nal(nal[0] & 0x1f)) {
        return;
    }
    const uint8_t hdr[4] = {(uint8_t)(len >> 24), (uint8_t)(len >> 16), (uint8_t)(len >> 8), (uint8_t)len};
    s->ok = s->io->write(s->io->io, hdr, 4) && s->io->write(s->io->io, nal, len);
}

/* ---- boxes --------------------------------------------------------------------- */

typedef struct {
    uint8_t *b;
    size_t len;
    size_t cap;
    bool bad;
} buf_t;

static void put(buf_t *o, const uint8_t *p, size_t n)
{
    if (o->bad) {
        return;
    }
    if (o->len + n > o->cap) {
        size_t cap = o->cap ? o->cap * 2 : 4096;
        while (cap < o->len + n) {
            cap *= 2;
        }
        uint8_t *g = realloc(o->b, cap);
        if (!g) {
            o->bad = true;
            return;
        }
        o->b = g;
        o->cap = cap;
    }
    memcpy(o->b + o->len, p, n);
    o->len += n;
}

static void u8(buf_t *o, uint8_t v) { put(o, &v, 1); }
static void u16(buf_t *o, uint16_t v) { const uint8_t b[2] = {v >> 8, v}; put(o, b, 2); }
static void u32(buf_t *o, uint32_t v)
{
    const uint8_t b[4] = {v >> 24, v >> 16, v >> 8, v};
    put(o, b, 4);
}
static void u64(buf_t *o, uint64_t v) { u32(o, (uint32_t)(v >> 32)); u32(o, (uint32_t)v); }
static void zeros(buf_t *o, size_t n) { while (n--) u8(o, 0); }

/* Open a box: its size is filled in by box_end. */
static size_t box(buf_t *o, const char *type)
{
    const size_t at = o->len;
    u32(o, 0);
    put(o, (const uint8_t *)type, 4);
    return at;
}

static void box_end(buf_t *o, size_t at)
{
    if (o->bad) {
        return;
    }
    const uint32_t n = (uint32_t)(o->len - at);
    o->b[at] = n >> 24;
    o->b[at + 1] = n >> 16;
    o->b[at + 2] = n >> 8;
    o->b[at + 3] = n;
}

static void full(buf_t *o, uint8_t version, uint32_t flags)
{
    u8(o, version);
    u8(o, flags >> 16);
    u8(o, flags >> 8);
    u8(o, flags);
}

static void matrix(buf_t *o)
{
    static const uint32_t m[9] = {0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x40000000};
    for (int i = 0; i < 9; i++) {
        u32(o, m[i]);
    }
}

/* ---- the conversion ------------------------------------------------------------ */

typedef struct {
    uint32_t *size;
    uint32_t *delta; /* duration of each sample, 90 kHz */
    uint32_t *offset; /* relative to mdat payload, for interleaved tracks */
    uint8_t *sync;
    size_t count;
    size_t cap;
} samples_t;

static bool sample_add(samples_t *s, uint32_t size, bool sync, uint32_t offset)
{
    if (s->count == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 4096;
        uint32_t *a = realloc(s->size, cap * sizeof(uint32_t));
        if (!a) return false;
        s->size = a;
        uint32_t *d = realloc(s->delta, cap * sizeof(uint32_t));
        if (!d) return false;
        s->delta = d;
        uint32_t *o = realloc(s->offset, cap * sizeof(uint32_t));
        if (!o) return false;
        s->offset = o;
        uint8_t *y = realloc(s->sync, cap);
        if (!y) return false;
        s->sync = y;
        s->cap = cap;
    }
    s->size[s->count] = size;
    s->offset[s->count] = offset;
    s->delta[s->count] = 0;
    s->sync[s->count] = sync;
    s->count++;
    return true;
}

typedef struct {
    uint8_t rate_index;
    uint8_t channels;
    uint32_t rate;
    size_t header_len;
} aac_t;

/* The recorder writes one AAC-LC raw block in each ADTS frame/PES. */
static bool aac_parse(const uint8_t *p, size_t n, aac_t *a)
{
    static const uint32_t rates[] = {96000, 88200, 64000, 48000, 44100, 32000,
                                     24000, 22050, 16000, 12000, 11025, 8000, 7350};
    if (n < 7 || p[0] != 0xff || (p[1] & 0xf6) != 0xf0 ||
        (p[2] >> 6) != 1 || (p[6] & 3)) return false;
    a->rate_index = (p[2] >> 2) & 15;
    a->channels = (uint8_t)(((p[2] & 1) << 2) | (p[3] >> 6));
    a->header_len = (p[1] & 1) ? 7 : 9;
    const size_t declared = ((size_t)(p[3] & 3) << 11) | ((size_t)p[4] << 3) | (p[5] >> 5);
    if (a->rate_index >= sizeof(rates) / sizeof(rates[0]) || a->channels < 1 ||
        a->channels > 2 || n <= a->header_len || n != declared) return false;
    a->rate = rates[a->rate_index];
    return true;
}

static void patch32(buf_t *o, size_t at, uint32_t n)
{
    if (o->bad) return;
    o->b[at] = n >> 24;
    o->b[at + 1] = n >> 16;
    o->b[at + 2] = n >> 8;
    o->b[at + 3] = n;
}

/* Audio/video movies use a 90 kHz movie clock as well, preserving sub-ms delay. */
static void delay_edit(buf_t *o, uint64_t delay, uint64_t duration)
{
    if (!delay) return;
    const size_t edts = box(o, "edts");
    const size_t elst = box(o, "elst");
    full(o, 0, 0);
    u32(o, 2);
    u32(o, (uint32_t)delay);
    u32(o, 0xffffffffu); /* empty edit until this track starts */
    u16(o, 1); u16(o, 0);
    u32(o, (uint32_t)duration);
    u32(o, 0);
    u16(o, 1); u16(o, 0);
    box_end(o, elst);
    box_end(o, edts);
}

static void sample_durations(buf_t *o, const samples_t *sm)
{
    const size_t stts = box(o, "stts");
    full(o, 0, 0);
    const size_t count_at = o->len;
    u32(o, 0);
    uint32_t runs = 0;
    for (size_t i = 0; i < sm->count;) {
        size_t j = i + 1;
        while (j < sm->count && sm->delta[j] == sm->delta[i]) j++;
        u32(o, (uint32_t)(j - i));
        u32(o, sm->delta[i]);
        runs++;
        i = j;
    }
    patch32(o, count_at, runs);
    box_end(o, stts);
}

/* Return the offset-table payload to patch once the complete header is known. */
static size_t audio_track(buf_t *o, const samples_t *sm, const aac_t *aac,
                          uint64_t duration, uint64_t delay)
{
    const size_t trak = box(o, "trak");
    size_t b = box(o, "tkhd");
    full(o, 0, 3);
    u32(o, 0); u32(o, 0); u32(o, 2); u32(o, 0);
    u32(o, (uint32_t)(duration + delay));
    zeros(o, 8);
    u16(o, 0); u16(o, 0); u16(o, 0x100); u16(o, 0);
    matrix(o);
    u32(o, 0); u32(o, 0);
    box_end(o, b);
    delay_edit(o, delay, duration);

    const size_t mdia = box(o, "mdia");
    b = box(o, "mdhd");
    full(o, 0, 0);
    u32(o, 0); u32(o, 0); u32(o, TIMESCALE); u32(o, (uint32_t)duration);
    u16(o, 0x55c4); u16(o, 0);
    box_end(o, b);
    b = box(o, "hdlr");
    full(o, 0, 0); u32(o, 0);
    put(o, (const uint8_t *)"soun", 4);
    zeros(o, 12);
    put(o, (const uint8_t *)"ESP-KVM audio\0", 14);
    box_end(o, b);

    const size_t minf = box(o, "minf");
    b = box(o, "smhd");
    full(o, 0, 0); u16(o, 0); u16(o, 0);
    box_end(o, b);
    const size_t dinf = box(o, "dinf");
    const size_t dref = box(o, "dref");
    full(o, 0, 0); u32(o, 1);
    b = box(o, "url "); full(o, 0, 1); box_end(o, b);
    box_end(o, dref); box_end(o, dinf);

    const size_t stbl = box(o, "stbl");
    const size_t stsd = box(o, "stsd");
    full(o, 0, 0); u32(o, 1);
    const size_t mp4a = box(o, "mp4a");
    zeros(o, 6); u16(o, 1); zeros(o, 8);
    u16(o, aac->channels); u16(o, 16); u16(o, 0); u16(o, 0);
    /* The ASC carries the full rate, including rates beyond 16.16's range. */
    u32(o, aac->rate <= 65535 ? aac->rate << 16 : 0);
    b = box(o, "esds");
    full(o, 0, 0);
    u8(o, 0x03); u8(o, 25); /* ES descriptor */
    u16(o, 2); u8(o, 0);
    u8(o, 0x04); u8(o, 17); /* DecoderConfig descriptor */
    u8(o, 0x40); u8(o, 0x15); /* MPEG-4 Audio, audio stream, reserved bit */
    u8(o, 0); u8(o, 0x20); u8(o, 0); /* maximum ADTS payload is under 8192 */
    u32(o, 0); u32(o, 0); /* bitrate unspecified */
    u8(o, 0x05); u8(o, 2); /* AudioSpecificConfig: AAC-LC, rate, channels */
    u8(o, (uint8_t)((2 << 3) | (aac->rate_index >> 1)));
    u8(o, (uint8_t)((aac->rate_index << 7) | (aac->channels << 3)));
    u8(o, 0x06); u8(o, 1); u8(o, 2); /* predefined MP4 SL configuration */
    box_end(o, b); box_end(o, mp4a); box_end(o, stsd);

    sample_durations(o, sm);
    b = box(o, "stsc");
    full(o, 0, 0); u32(o, 1); u32(o, 1); u32(o, 1); u32(o, 1);
    box_end(o, b);
    b = box(o, "stsz");
    full(o, 0, 0); u32(o, 0); u32(o, (uint32_t)sm->count);
    for (size_t i = 0; i < sm->count; i++) u32(o, sm->size[i]);
    box_end(o, b);
    b = box(o, "stco");
    full(o, 0, 0); u32(o, (uint32_t)sm->count);
    const size_t offsets_at = o->len;
    for (size_t i = 0; i < sm->count; i++) u32(o, sm->offset[i]);
    box_end(o, b);
    box_end(o, stbl); box_end(o, minf); box_end(o, mdia); box_end(o, trak);
    return offsets_at;
}

bool ts_to_mp4(const mp4_io_t *io, uint32_t width, uint32_t height, const mp4_chapter_t *chapters,
               int chapter_count, char *err, size_t err_cap)
{
    samples_t sm = {0}, am = {0};
    aac_t audio = {0};
    nal_scan_t params = {0};
    buf_t head = {0};
    ts_reader_t rd = {.io = io};
    bool ok = false;
    uint64_t data_bytes = 0;
    int64_t first_pts = -1, prev_pts = -1;
    int64_t audio_first = -1, audio_prev = -1;
    int64_t segment_offset = 0, video_raw_prev = -1;

    /* Pass one: the sample table. */
    int got;
    while ((got = next_au(&rd)) == 1) {
        const uint8_t *au = rd.ready.b;
        const size_t au_len = rd.ready.len;
        int64_t pts = rd.ready.pts + segment_offset;
        if (rd.ready.audio) {
            aac_t parsed;
            if (!aac_parse(au, au_len, &parsed) ||
                (am.count && (parsed.rate_index != audio.rate_index || parsed.channels != audio.channels))) {
                snprintf(err, err_cap, "invalid, unsupported, or changing AAC format");
                goto out;
            }
            audio = parsed;
            if (!am.count) audio_first = pts;
            if (am.count) {
                /* Keep capture gaps on the movie timeline instead of closing
                 * them up and moving later sound ahead of its video. */
                const int64_t d = pts - audio_prev;
                if (d <= 0 || d > UINT32_MAX) {
                    snprintf(err, err_cap, "non-monotonic AAC timestamps");
                    goto out;
                }
                am.delta[am.count - 1] = (uint32_t)d;
            }
            audio_prev = pts;
            if (data_bytes > UINT32_MAX ||
                !sample_add(&am, (uint32_t)(au_len - parsed.header_len), true, (uint32_t)data_bytes)) {
                got = -1;
                break;
            }
            data_bytes += au_len - parsed.header_len;
            continue;
        }
        nal_scan_t s = {.sps = params.sps, .pps = params.pps,
                        .sps_len = params.sps_len, .pps_len = params.pps_len};
        each_nal(au, au_len, scan_nal, &s);
        params.sps = s.sps;
        params.sps_len = s.sps_len;
        params.pps = s.pps;
        params.pps_len = s.pps_len;
        if (!s.size) {
            continue;
        }
        if (!sm.count && !s.idr) {
            continue; /* a file has to start on a picture that decodes on its own */
        }
        if (sm.count && rd.ready.pts <= video_raw_prev) {
            const int64_t raw_delta = rd.ready.pts - video_raw_prev;
            if (raw_delta < -(1ll << 32)) {
                segment_offset += 1ll << 33; /* the MPEG timestamp wrapped */
            } else {
                /* Concatenated dashcam segments restart their common clock.
                 * Their first video PES precedes audio in each new segment. */
                const uint32_t before = sm.count > 1 ? sm.delta[sm.count - 2] : TIMESCALE / 25;
                segment_offset = prev_pts + before - rd.ready.pts;
            }
            pts = rd.ready.pts + segment_offset;
        }
        video_raw_prev = rd.ready.pts;
        if (first_pts < 0) first_pts = pts;
        if (sm.count) {
            /* Keep long capture gaps in the common timeline; segment resets
             * were already normalized above. */
            const int64_t d = pts - prev_pts;
            const uint32_t before = sm.count > 1 ? sm.delta[sm.count - 2] : TIMESCALE / 25;
            sm.delta[sm.count - 1] = (d > 0 && d <= UINT32_MAX) ? (uint32_t)d : before;
        }
        prev_pts = pts;
        if (data_bytes > UINT32_MAX || !sample_add(&sm, (uint32_t)s.size, s.idr, (uint32_t)data_bytes)) {
            got = -1;
            break;
        }
        data_bytes += s.size;
    }
    if (got < 0) {
        snprintf(err, err_cap, "%s", got == -2 ? "malformed or truncated audio PES" : "out of memory reading the recording");
        goto out;
    }
    if (!sm.count || !params.sps || !params.pps) {
        snprintf(err, err_cap, "the recording has no complete H.264 picture");
        goto out;
    }
    sm.delta[sm.count - 1] = sm.count > 1 ? sm.delta[sm.count - 2] : TIMESCALE / 25;
    if (data_bytes + 8 > 0xffffffffull) {
        snprintf(err, err_cap, "too long for one MP4 here (over 4 GB)");
        goto out;
    }
    uint64_t duration = 0;
    for (size_t i = 0; i < sm.count; i++) {
        duration += sm.delta[i];
    }

    uint64_t audio_duration = 0;
    if (am.count) {
        am.delta[am.count - 1] = (uint32_t)((1024ull * TIMESCALE + audio.rate / 2) / audio.rate);
        for (size_t i = 0; i < am.count; i++) audio_duration += am.delta[i];
    }
    const int64_t origin = am.count && audio_first < first_pts ? audio_first : first_pts;
    const uint64_t video_delay = (uint64_t)(first_pts - origin);
    const uint64_t audio_delay = am.count ? (uint64_t)(audio_first - origin) : 0;
    uint64_t movie_duration = duration + video_delay;
    if (am.count && audio_duration + audio_delay > movie_duration) movie_duration = audio_duration + audio_delay;
    if (movie_duration > UINT32_MAX) {
        snprintf(err, err_cap, "too long for the MP4 timeline");
        goto out;
    }
    const uint32_t movie_scale = am.count ? TIMESCALE : 1000;

    /* The index, written before the samples so the file plays as it arrives. */
    size_t b = box(&head, "ftyp");
    put(&head, (const uint8_t *)"isom", 4);
    u32(&head, 0x200);
    put(&head, (const uint8_t *)"isomiso2avc1mp41", 16);
    box_end(&head, b);

    const size_t moov = box(&head, "moov");
    b = box(&head, "mvhd");
    full(&head, 0, 0);
    u32(&head, 0);
    u32(&head, 0);
    u32(&head, movie_scale);
    u32(&head, (uint32_t)(movie_duration * movie_scale / TIMESCALE));
    u32(&head, 0x10000);
    u16(&head, 0x100);
    zeros(&head, 10);
    matrix(&head);
    zeros(&head, 24);
    u32(&head, am.count ? 3 : 2);
    box_end(&head, b);

    const size_t trak = box(&head, "trak");
    b = box(&head, "tkhd");
    full(&head, 0, 3);
    u32(&head, 0);
    u32(&head, 0);
    u32(&head, 1);
    u32(&head, 0);
    u32(&head, (uint32_t)((duration + video_delay) * movie_scale / TIMESCALE));
    zeros(&head, 8);
    u16(&head, 0);
    u16(&head, 0);
    u16(&head, 0);
    u16(&head, 0);
    matrix(&head);
    u32(&head, width << 16);
    u32(&head, height << 16);
    box_end(&head, b);

    delay_edit(&head, video_delay, duration);
    const size_t mdia = box(&head, "mdia");
    b = box(&head, "mdhd");
    full(&head, 0, 0);
    u32(&head, 0);
    u32(&head, 0);
    u32(&head, TIMESCALE);
    u32(&head, (uint32_t)duration);
    u16(&head, 0x55c4); /* "und" */
    u16(&head, 0);
    box_end(&head, b);
    b = box(&head, "hdlr");
    full(&head, 0, 0);
    u32(&head, 0);
    put(&head, (const uint8_t *)"vide", 4);
    zeros(&head, 12);
    put(&head, (const uint8_t *)"ESP-KVM\0", 8);
    box_end(&head, b);

    const size_t minf = box(&head, "minf");
    b = box(&head, "vmhd");
    full(&head, 0, 1);
    zeros(&head, 8);
    box_end(&head, b);
    const size_t dinf = box(&head, "dinf");
    const size_t dref = box(&head, "dref");
    full(&head, 0, 0);
    u32(&head, 1);
    b = box(&head, "url ");
    full(&head, 0, 1);
    box_end(&head, b);
    box_end(&head, dref);
    box_end(&head, dinf);

    const size_t stbl = box(&head, "stbl");
    const size_t stsd = box(&head, "stsd");
    full(&head, 0, 0);
    u32(&head, 1);
    const size_t avc1 = box(&head, "avc1");
    zeros(&head, 6);
    u16(&head, 1);
    zeros(&head, 16);
    u16(&head, (uint16_t)width);
    u16(&head, (uint16_t)height);
    u32(&head, 0x480000);
    u32(&head, 0x480000);
    u32(&head, 0);
    u16(&head, 1);
    zeros(&head, 32);
    u16(&head, 0x18);
    u16(&head, 0xffff);
    b = box(&head, "avcC");
    u8(&head, 1);
    u8(&head, params.sps[1]);
    u8(&head, params.sps[2]);
    u8(&head, params.sps[3]);
    u8(&head, 0xff); /* four-byte NAL lengths */
    u8(&head, 0xe1); /* one SPS */
    u16(&head, (uint16_t)params.sps_len);
    put(&head, params.sps, params.sps_len);
    u8(&head, 1);
    u16(&head, (uint16_t)params.pps_len);
    put(&head, params.pps, params.pps_len);
    box_end(&head, b);
    box_end(&head, avc1);
    box_end(&head, stsd);

    sample_durations(&head, &sm);

    const size_t stss = box(&head, "stss");
    full(&head, 0, 0);
    uint32_t syncs = 0;
    for (size_t i = 0; i < sm.count; i++) {
        syncs += sm.sync[i];
    }
    u32(&head, syncs);
    for (size_t i = 0; i < sm.count; i++) {
        if (sm.sync[i]) {
            u32(&head, (uint32_t)(i + 1));
        }
    }
    box_end(&head, stss);

    b = box(&head, "stsc");
    full(&head, 0, 0);
    u32(&head, 1);
    u32(&head, 1);
    u32(&head, am.count ? 1 : (uint32_t)sm.count);
    u32(&head, 1);
    box_end(&head, b);

    b = box(&head, "stsz");
    full(&head, 0, 0);
    u32(&head, 0);
    u32(&head, (uint32_t)sm.count);
    for (size_t i = 0; i < sm.count; i++) {
        u32(&head, sm.size[i]);
    }
    box_end(&head, b);

    const size_t stco = box(&head, "stco");
    full(&head, 0, 0);
    const size_t video_chunks = am.count ? sm.count : 1;
    u32(&head, (uint32_t)video_chunks);
    const size_t offset_at = head.len;
    for (size_t i = 0; i < video_chunks; i++) u32(&head, sm.offset[i]);
    box_end(&head, stco);
    box_end(&head, stbl);
    box_end(&head, minf);
    box_end(&head, mdia);
    box_end(&head, trak);

    const size_t audio_offset_at = am.count ? audio_track(&head, &am, &audio, audio_duration, audio_delay) : 0;

    /* Chapters, the way Nero wrote them and VLC, mpv and ffmpeg read them. */
    if (chapter_count > 0 && chapters[0].at_us < (int64_t)(duration * 1000000 / TIMESCALE)) {
        const size_t udta = box(&head, "udta");
        b = box(&head, "chpl");
        full(&head, 1, 0);
        u32(&head, 0);
        /* Only chapters inside the video: a cut-off recording can be shorter
         * than the moments it was meant to hold. */
        const int64_t length_us = (int64_t)(duration * 1000000 / TIMESCALE);
        int n = 0;
        for (int i = 0; i < chapter_count && n < 255; i++) {
            n += chapters[i].at_us < length_us;
        }
        u8(&head, (uint8_t)n);
        for (int i = 0, kept = 0; i < chapter_count && kept < n; i++) {
            if (chapters[i].at_us >= length_us) {
                continue;
            }
            kept++;
            int64_t t = chapters[i].at_us;
            if (t < 0) {
                t = 0;
            }
            u64(&head, (uint64_t)t * 10 + video_delay * 10000000 / TIMESCALE); /* 100 ns units */
            const char *title = chapters[i].title ? chapters[i].title : "";
            size_t tl = strlen(title);
            if (tl > 255) {
                tl = 255;
            }
            u8(&head, (uint8_t)tl);
            put(&head, (const uint8_t *)title, tl);
        }
        box_end(&head, b);
        box_end(&head, udta);
    }
    box_end(&head, moov);

    if (head.bad) {
        snprintf(err, err_cap, "out of memory building the MP4 index");
        goto out;
    }
    if (head.len + 8 + data_bytes > UINT32_MAX) {
        snprintf(err, err_cap, "too long for one MP4 here (over 4 GB)");
        goto out;
    }
    const uint32_t data_at = (uint32_t)(head.len + 8);
    for (size_t i = 0; i < video_chunks; i++) patch32(&head, offset_at + i * 4, data_at + sm.offset[i]);
    for (size_t i = 0; i < am.count; i++) patch32(&head, audio_offset_at + i * 4, data_at + am.offset[i]);

    const uint8_t mdat[8] = {(uint8_t)((data_bytes + 8) >> 24), (uint8_t)((data_bytes + 8) >> 16),
                             (uint8_t)((data_bytes + 8) >> 8), (uint8_t)(data_bytes + 8),
                             'm', 'd', 'a', 't'};
    if (!io->write(io->io, head.b, head.len) || !io->write(io->io, mdat, 8)) {
        snprintf(err, err_cap, "could not write the MP4");
        goto out;
    }

    /* Pass two: the samples, in the same order the table counted them. */
    if (!io->rewind(io->io)) {
        snprintf(err, err_cap, "could not read the recording again");
        goto out;
    }
    reader_free(&rd);
    rd = (ts_reader_t){.io = io};
    size_t written = 0, audio_written = 0;
    bool started = false;
    while ((got = next_au(&rd)) == 1) {
        const uint8_t *au = rd.ready.b;
        const size_t au_len = rd.ready.len;
        if (rd.ready.audio) {
            aac_t parsed;
            if (audio_written >= am.count || !aac_parse(au, au_len, &parsed) ||
                parsed.rate_index != audio.rate_index || parsed.channels != audio.channels ||
                au_len - parsed.header_len != am.size[audio_written]) {
                snprintf(err, err_cap, "the recording changed while it was converted");
                goto out;
            }
            if (!io->write(io->io, au + parsed.header_len, au_len - parsed.header_len)) {
                snprintf(err, err_cap, "could not write the MP4 audio");
                goto out;
            }
            audio_written++;
            continue;
        }
        nal_scan_t probe = {.sps = params.sps, .pps = params.pps,
                            .sps_len = params.sps_len, .pps_len = params.pps_len};
        each_nal(au, au_len, scan_nal, &probe);
        if (!probe.size || (!started && !probe.idr)) {
            continue;
        }
        if (written >= sm.count || probe.size != sm.size[written]) {
            snprintf(err, err_cap, "the recording changed while it was converted");
            goto out;
        }
        started = true;
        nal_scan_t w = {.io = io, .ok = true};
        each_nal(au, au_len, write_nal, &w);
        if (!w.ok) {
            snprintf(err, err_cap, "could not write the MP4");
            goto out;
        }
        written++;
    }
    if (got < 0 || written != sm.count || audio_written != am.count) {
        snprintf(err, err_cap, "the recording changed while it was converted");
        goto out;
    }
    ok = true;

out:
    reader_free(&rd);
    free(sm.size);
    free(sm.delta);
    free(sm.sync);
    free(sm.offset);
    free(am.size);
    free(am.delta);
    free(am.sync);
    free(am.offset);
    free(params.sps);
    free(params.pps);
    free(head.b);
    return ok;
}
