#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 ESP-KVM contributors
# SPDX-License-Identifier: Apache-2.0
"""Optional real-codec TS/MP4 validation; requires ffmpeg, ffprobe and a C compiler."""
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def run(*args):
    return subprocess.check_output([str(arg) for arg in args], stderr=subprocess.PIPE)


def probe(path, *args):
    return json.loads(run("ffprobe", "-v", "error", *args, "-of", "json", path))


def main():
    with tempfile.TemporaryDirectory(prefix="espkvm-aac-roundtrip-") as tmp:
        tmp = Path(tmp)
        video, audio = tmp / "video.h264", tmp / "audio.aac"
        run("ffmpeg", "-v", "error", "-f", "lavfi", "-i", "testsrc2=size=320x240:rate=25",
            "-t", "2", "-c:v", "libx264", "-preset", "ultrafast", "-tune", "zerolatency",
            "-x264-params", "aud=1:keyint=25", "-an", "-f", "h264", video)
        run("ffmpeg", "-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
            "-t", "1.7", "-ac", "2", "-c:a", "aac", "-b:a", "128k", "-f", "adts", audio)
        raw_video = video.read_bytes()
        starts = [match.start() for match in re.finditer(b"\x00\x00\x00\x01\x09", raw_video)]
        assert starts and starts[0] == 0
        starts.append(len(raw_video))
        records = [(i * 3600, 0, raw_video[starts[i]:starts[i + 1]]) for i in range(len(starts) - 1)]
        raw_audio = audio.read_bytes()
        at, count = 0, 0
        expected_audio_pts = []
        while at < len(raw_audio):
            size = ((raw_audio[at + 3] & 3) << 11) | (raw_audio[at + 4] << 3) | (raw_audio[at + 5] >> 5)
            assert size >= 7 and at + size <= len(raw_audio)
            # Delay audio 100 ms; leave a one-frame capture gap after frame 34.
            pts = 9000 + count * 1920 + (1920 if count >= 35 else 0)
            expected_audio_pts.append(pts)
            records.append((pts, 1, raw_audio[at:at + size]))
            count += 1
            at += size
        fixture = tmp / "frames.bin"
        with fixture.open("wb") as out:
            for pts, kind, data in sorted(records):
                out.write(struct.pack("<BIQ", kind, len(data), pts))
                out.write(data)
        driver = tmp / "remux_fixture"
        run(os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-I", HERE / "../include", HERE / "remux_fixture.c", HERE / "../ts_mux.c",
            HERE / "../ts_to_mp4.c", "-o", driver)
        ts, mp4 = tmp / "av.ts", tmp / "av.mp4"
        run(driver, fixture, ts, mp4)
        for path in (ts, mp4):
            streams = probe(path, "-show_streams")["streams"]
            assert [stream["codec_name"] for stream in streams] == ["h264", "aac"], streams
            a = streams[1]
            assert a["profile"] == "LC" and a["sample_rate"] == "48000" and a["channels"] == 2, a
            run("ffmpeg", "-v", "error", "-xerror", "-i", path, "-map", "0:v", "-map", "0:a", "-f", "null", "-")
        packets = probe(mp4, "-select_streams", "a", "-show_packets")["packets"]
        assert len(packets) == count, (len(packets), count)
        # MP4 uses the common 90 kHz clock and retains both the initial offset and gap.
        assert [int(packet["pts"]) for packet in packets] == expected_audio_pts
        decoded_original = run("ffmpeg", "-v", "error", "-i", audio, "-f", "s16le", "-")
        decoded_mp4 = run("ffmpeg", "-v", "error", "-i", mp4, "-map", "0:a", "-f", "s16le", "-")
        assert decoded_original == decoded_mp4, "AAC samples changed during remux"
        segments = []
        for segment in range(2):
            base = segment * 90000
            segment_records = [(pts - base, kind, data) for pts, kind, data in records
                               if base <= pts < base + 90000]
            part = tmp / f"part{segment}.bin"
            with part.open("wb") as out:
                for pts, kind, data in sorted(segment_records):
                    out.write(struct.pack("<BIQ", kind, len(data), pts))
                    out.write(data)
            part_ts, part_mp4 = tmp / f"part{segment}.ts", tmp / f"part{segment}.mp4"
            run(driver, part, part_ts, part_mp4)
            segments.append(part_ts.read_bytes())
            expected = [pts for pts, kind, _ in sorted(segment_records) if kind == 1]
            actual = probe(part_mp4, "-select_streams", "a", "-show_packets")["packets"]
            assert [int(packet["pts"]) for packet in actual] == expected
        joined_ts, joined_mp4 = tmp / "joined.ts", tmp / "joined.mp4"
        joined_ts.write_bytes(b"".join(segments))
        run(driver, "--remux", joined_ts, joined_mp4)
        actual = probe(joined_mp4, "-select_streams", "a", "-show_packets")["packets"]
        assert [int(packet["pts"]) for packet in actual] == expected_audio_pts
        joined_pcm = run("ffmpeg", "-v", "error", "-xerror", "-i", joined_mp4, "-map", "0:a", "-f", "s16le", "-")
        assert joined_pcm == decoded_original
        print(f"audio round trip: H.264 + {count} AAC frames decode in TS/MP4; offsets, gap, PCM, and split/join clocks preserved")


if __name__ == "__main__":
    main()
