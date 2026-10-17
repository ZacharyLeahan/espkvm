/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define VIDEO_ACK_WINDOW 2
#define VIDEO_ACK_TIMEOUT_US 500000
/* Protected by the video client mutex. Legacy viewers never enable the window.
 * Keep sequence IDs even before negotiation so only an ACK for a sent frame
 * can enable it; duplicate/stale ACKs cannot release unrelated frames. */
typedef struct {
    bool enabled;
    unsigned count;
    uint32_t seq[VIDEO_ACK_WINDOW];
    int64_t sent_us[VIDEO_ACK_WINDOW];
} video_ack_t;

static inline void video_ack_sent(video_ack_t *a, uint32_t seq, int64_t now)
{
    if (a->count == VIDEO_ACK_WINDOW) {
        a->seq[0] = a->seq[1];
        a->sent_us[0] = a->sent_us[1];
        a->count--;
    }
    a->seq[a->count] = seq;
    a->sent_us[a->count++] = now;
}

static inline bool video_ack_received(video_ack_t *a, uint32_t seq)
{
    for (unsigned i = 0; i < a->count; i++) {
        if (a->seq[i] != seq) continue;
        a->enabled = true;
        for (unsigned j = i + 1; j < a->count; j++) {
            a->seq[j - 1] = a->seq[j];
            a->sent_us[j - 1] = a->sent_us[j];
        }
        a->count--;
        return true;
    }
    return false;
}

static inline bool video_ack_blocked(video_ack_t *a, int64_t now)
{
    if (!a->enabled) return false;
    /* Expire by the oldest send, not the latest, so steady traffic cannot
     * indefinitely retain a lost acknowledgement. */
    while (a->count && now - a->sent_us[0] >= VIDEO_ACK_TIMEOUT_US) {
        for (unsigned j = 1; j < a->count; j++) {
            a->seq[j - 1] = a->seq[j];
            a->sent_us[j - 1] = a->sent_us[j];
        }
        a->count--;
    }
    return a->count >= VIDEO_ACK_WINDOW;
}
