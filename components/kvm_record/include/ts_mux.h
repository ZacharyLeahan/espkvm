/*
 * SPDX-FileCopyrightText: 2026 ESP-KVM contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * H.264 and optional AAC-LC into an MPEG transport stream.
 *
 * A recording is written as .ts rather than .mp4 because an MP4 keeps its index
 * at the end: pull the card or lose power and the file does not open. A
 * transport stream is a run of 188-byte packets that a player can start reading
 * anywhere, so a cut-off recording plays up to where it stopped.
 *
 * Plain C over a byte callback, so it is tested on a host.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TS_PACKET 188

/** Called with every finished 188-byte packet, in order. */
typedef void (*ts_mux_out_t)(void *user, const uint8_t *packet);

typedef struct {
    ts_mux_out_t out;
    void *user;
    uint8_t cc_pat;
    uint8_t cc_pmt;
    uint8_t cc_video;
    uint8_t cc_audio;
    bool audio_enabled;
    uint8_t audio_rate_index;
    uint8_t audio_channels;
} ts_mux_t;

void ts_mux_init(ts_mux_t *m, ts_mux_out_t out, void *user);

/** Configure before writing the first packet; reapply after each init/split. */
void ts_mux_set_audio(ts_mux_t *m, bool enabled);

/**
 * One complete AAC-LC ADTS frame (1024 samples/channel, mono or stereo).
 * The timestamp uses the same recording-relative 90 kHz clock as video; the
 * muxer adds the same decoder lead to both. Calls must be serialized with
 * ts_mux_frame. Returns false without writing if disabled, malformed, or if
 * the ADTS sample rate/channel configuration changes within this stream.
 */
bool ts_mux_audio_adts(ts_mux_t *m, const uint8_t *data, size_t len, uint64_t pts_90k);

/**
 * One H.264 access unit (Annex-B, as the encoder gives it) at @p pts_90k, in
 * 90 kHz units. A keyframe is preceded by the stream tables, so a player can
 * start there.
 */
void ts_mux_frame(ts_mux_t *m, const uint8_t *data, size_t len, uint64_t pts_90k, bool keyframe);

#ifdef __cplusplus
}
#endif
