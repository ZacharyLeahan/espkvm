#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Measure MJPEG delivery and reconnects without saving private video frames."""
import argparse
import json
import time
import urllib.request


class JpegMarkers:
    """Count start/end pairs across arbitrary network read boundaries."""
    def __init__(self):
        self.tail = b""
        self.in_frame = False

    def feed(self, chunk):
        data = self.tail + chunk
        pos = count = 0
        while True:
            marker = b"\xff\xd9" if self.in_frame else b"\xff\xd8"
            found = data.find(marker, pos)
            if found < 0:
                break
            pos = found + 2
            if self.in_frame:
                count += 1
            self.in_frame = not self.in_frame
        self.tail = data[-1:] if pos < len(data) else b""
        return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("url", help="Local /stream URL")
    parser.add_argument("--seconds", type=int, default=600)
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    start = last_report = last_frame = time.monotonic()
    deadline = start + args.seconds
    frames = octets = reconnects = interval_frames = 0
    longest_gap = 0.0

    def report(event, **extra):
        print(json.dumps(dict(event=event, elapsed=round(time.monotonic()-start, 1),
                              frames=frames, bytes=octets, reconnects=reconnects,
                              longest_gap_s=round(longest_gap, 2), **extra)), flush=True)

    while time.monotonic() < deadline:
        try:
            with opener.open(args.url, timeout=8) as response:
                if "multipart/x-mixed-replace" not in response.headers.get("Content-Type", ""):
                    raise ValueError("Not an MJPEG response")
                report("connected")
                connected_at = time.monotonic()
                markers = JpegMarkers()
                while time.monotonic() < deadline:
                    chunk = response.read1(16384)
                    if not chunk:
                        raise EOFError("Stream closed")
                    octets += len(chunk)
                    count = markers.feed(chunk)
                    if count:
                        now = time.monotonic()
                        longest_gap = max(longest_gap, now-last_frame)
                        last_frame = now
                        frames += count
                    now = time.monotonic()
                    if now-max(last_frame, connected_at) > 15:
                        raise TimeoutError("No complete JPEG for 15 seconds")
                    if now-last_report >= 30:
                        report("sample", fps=round((frames-interval_frames)/(now-last_report), 2))
                        last_report, interval_frames = now, frames
        except (OSError, ValueError, EOFError) as exc:
            reconnects += 1
            longest_gap = max(longest_gap, time.monotonic()-last_frame)
            report("interruption", error=str(exc))
            time.sleep(min(1, max(0, deadline-time.monotonic())))
    report("finished", average_fps=round(frames/(time.monotonic()-start), 2))


if __name__ == "__main__":
    main()
