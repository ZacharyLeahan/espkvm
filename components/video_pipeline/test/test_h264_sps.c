// SPDX-License-Identifier: Apache-2.0
/*
 * The device's SPS fix, on a real SPS from a Function EV at 1080p: the result
 * must be byte for byte what the console's own patch (checked by a separate
 * parser) makes, the rest of the access unit must not move, and a second
 * pass must change nothing.
 */
#include <stdio.h>
#include <string.h>

#include "h264_sps.h"

static int fails;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);        \
            fails++;                                                   \
        }                                                              \
    } while (0)

static size_t from_hex(const char *s, unsigned char *out)
{
    size_t n = 0;
    while (s[0] && s[1]) {
        unsigned v;
        sscanf(s, "%2x", &v);
        out[n++] = (unsigned char)v;
        s += 2;
    }
    return n;
}

int main(void)
{
    unsigned char sps[64], want[64], pps[8], idr[8], au[256];
    const size_t sps_n = from_hex("6742c02995a01e008979610000030001000003003c84", sps);
    const size_t want_n = from_hex("6742c02995a01e008979610000030001000003003c8f08846a", want);
    const size_t pps_n = from_hex("68ce3c80", pps);
    const size_t idr_n = from_hex("6588840021ff", idr);
    const unsigned char sc[4] = {0, 0, 0, 1};

    size_t len = 0;
    memcpy(au + len, sc, 4), len += 4;
    memcpy(au + len, sps, sps_n), len += sps_n;
    memcpy(au + len, sc, 4), len += 4;
    memcpy(au + len, pps, pps_n), len += pps_n;
    memcpy(au + len, sc, 4), len += 4;
    memcpy(au + len, idr, idr_n), len += idr_n;

    const size_t out = h264_sps_add_restriction(au, len, sizeof(au));
    CHECK(out == len - sps_n + want_n);
    CHECK(memcmp(au + 4, want, want_n) == 0);
    CHECK(memcmp(au + 4 + want_n, sc, 4) == 0);
    CHECK(memcmp(au + 8 + want_n, pps, pps_n) == 0);
    CHECK(memcmp(au + out - idr_n, idr, idr_n) == 0);
    CHECK(h264_sps_add_restriction(au, out, sizeof(au)) == out);

    /* No room: left alone. */
    unsigned char tight[64];
    memcpy(tight, sc, 4);
    memcpy(tight + 4, sps, sps_n);
    CHECK(h264_sps_add_restriction(tight, 4 + sps_n, 4 + sps_n) == 4 + sps_n);

    /* A P-frame: nothing to do. */
    unsigned char p[] = {0, 0, 0, 1, 0x41, 0x9a, 0x02};
    CHECK(h264_sps_add_restriction(p, sizeof(p), sizeof(p) + 32) == sizeof(p));

    /* Invalid capacity and oversized SPS must leave the caller's bytes alone. */
    CHECK(h264_sps_add_restriction(NULL, 10, 20) == 10);
    unsigned char large[256], saved[256];
    memset(large, 0x55, sizeof(large));
    memcpy(large, sc, 4);
    memcpy(large + 4, sps, sps_n);
    memcpy(saved, large, sizeof(large));
    CHECK(h264_sps_add_restriction(large, 150, sizeof(large)) == 150);
    CHECK(memcmp(large, saved, sizeof(large)) == 0);
    CHECK(h264_sps_add_restriction(large, 150, 10) == 150);
    CHECK(memcmp(large, saved, sizeof(large)) == 0);

    /* Exercise every truncation and bounded malformed SPS payload under ASan.
       Bytes outside the advertised capacity must never be changed. */
    uint32_t seed = 12345;
    for (size_t trial = 0; trial < 10000; trial++) {
        memset(large, 0xa5, sizeof(large));
        size_t cap = trial % 128;
        size_t n = cap ? trial % (cap + 1) : 0;
        for (size_t i = 0; i < n; i++) {
            seed = seed * 1664525u + 1013904223u;
            large[i] = (unsigned char)(seed >> 24);
        }
        if (n >= 5) { memcpy(large, sc, 4); large[4] = 0x67; }
        if (trial < sps_n + 4) {
            memcpy(large, sc, 4); memcpy(large + 4, sps, sps_n);
            n = trial; cap = trial;
        }
        memcpy(saved, large, sizeof(large));
        const size_t result = h264_sps_add_restriction(large, n, cap);
        CHECK(result <= cap);
        CHECK(memcmp(large + cap, saved + cap, sizeof(large) - cap) == 0);
    }

    printf(fails ? "%d FAILED\n" : "h264 sps: all passed\n", fails);
    return fails ? 1 : 0;
}
