/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "video_ack.h"

int main(void)
{
    video_ack_t a = {0};
    for (uint32_t i = 0; i < 100; i++) {
        assert(!video_ack_blocked(&a, i)); /* legacy viewers are not throttled */
        video_ack_sent(&a, i, i);
    }
    assert(!video_ack_received(&a, 5)); /* no activation by an old sequence */
    assert(video_ack_received(&a, 98));
    assert(a.enabled && a.count == 1);
    assert(!video_ack_received(&a, 98)); /* duplicate cannot drain another */
    video_ack_sent(&a, 100, 100);
    assert(video_ack_blocked(&a, 101));
    assert(video_ack_received(&a, 100)); /* out-of-order exact ACK */
    assert(a.count == 1 && a.seq[0] == 99);
    video_ack_sent(&a, 101, 200);
    assert(video_ack_blocked(&a, 500098));
    assert(!video_ack_blocked(&a, 500099)); /* oldest expires first */
    assert(a.count == 1 && a.seq[0] == 101);
    assert(!video_ack_received(&a, 99));
    assert(!video_ack_blocked(&a, 500200));
    assert(a.count == 0);
    video_ack_sent(&a, UINT32_MAX, 600000);
    video_ack_sent(&a, 0, 600001);
    assert(video_ack_blocked(&a, 600002));
    assert(video_ack_received(&a, UINT32_MAX));
    assert(video_ack_received(&a, 0));
    a = (video_ack_t){0}; /* disconnected client slot */
    assert(!video_ack_received(&a, 0) && !a.enabled);
    puts("video ACK window: negotiation, duplicate/stale ACKs, expiry and wrap passed");
}
