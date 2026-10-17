#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Capture bounded HDMI PCM evidence and a WAV for game testing (one listener)."""
import argparse
import http.cookiejar
import http.client
import json
from pathlib import Path
import ssl
import struct
import time
import urllib.parse
import urllib.request
import wave


def levels(data):
    samples = [item[0] / 32768 for item in struct.iter_unpack('<h', data)]
    return {'peak': max(map(abs, samples), default=0),
            'rms': (sum(x * x for x in samples) / len(samples)) ** 0.5 if samples else 0,
            'active_frames': sum(abs(samples[i]) > 32 / 32768 or abs(samples[i + 1]) > 32 / 32768
                                 for i in range(0, len(samples), 2))}


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('url', help='Device origin, such as http://esp.local')
    parser.add_argument('--seconds', type=float, default=5)
    parser.add_argument('--output', required=True, type=Path, help='New WAV file; will not overwrite')
    parser.add_argument('--cookies', type=Path, help='Existing Netscape cookie file')
    parser.add_argument('--ca', help='Device TLS certificate file')
    args = parser.parse_args(argv)
    url = urllib.parse.urlsplit(args.url)
    if (url.scheme not in ('http', 'https') or not url.hostname or url.username or url.password
            or url.query or url.fragment or url.path not in ('', '/')):
        parser.error('URL must be an HTTP(S) origin without credentials, query or path')
    if not 0 < args.seconds <= 600:
        parser.error('Duration must be between 0 and 600 seconds')
    cookies = http.cookiejar.MozillaCookieJar()
    if args.cookies:
        cookies.load(str(args.cookies), ignore_discard=True)
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect(),
        urllib.request.HTTPCookieProcessor(cookies),
        urllib.request.HTTPSHandler(context=ssl.create_default_context(cafile=args.ca)))
    start = time.monotonic()
    frames = active = 0
    peak = energy = 0.0
    # read1 returns available bytes, so missing source clocks cannot fill a long
    # blocking read and confuse the requested observation duration.
    with opener.open(args.url.rstrip('/') + '/audio.pcm', timeout=2) as response:
        rate = int(response.headers.get('X-Audio-Rate', '0'))
        if rate not in (32000, 44100, 48000, 88200, 96000) or response.headers.get('X-Audio-Format') != 's16le-stereo':
            raise ValueError('Unexpected audio format')
        with args.output.open('xb') as out, wave.open(out, 'wb') as wav:
            wav.setnchannels(2); wav.setsampwidth(2); wav.setframerate(rate)
            carry = b''
            limit = int(args.seconds * rate)
            while frames < limit and time.monotonic() - start < args.seconds:
                chunk = response.read1(min(8192, (limit - frames) * 4))
                if not chunk:
                    raise ValueError('Audio stream ended before observation completed')
                data = carry + chunk
                aligned = len(data) // 4 * 4
                carry = data[aligned:]
                data = data[:aligned]
                if not data:
                    continue
                wav.writeframesraw(data)
                item = levels(data)
                count = len(data) // 4
                frames += count; active += item['active_frames']
                peak = max(peak, item['peak']); energy += item['rms'] ** 2 * count
    summary = dict(frames=frames, rate=rate, audio_seconds=frames / rate,
                   wall_seconds=round(time.monotonic() - start, 3), peak=peak,
                   rms=(energy / frames) ** 0.5 if frames else 0, active_frames=active,
                   observation='sound_detected' if active else 'quiet_samples' if frames else 'no_samples')
    print(json.dumps(summary))
    return 0 if frames else 1


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, http.client.HTTPException) as error:
        # Avoid logging credentials, private response bodies or request URLs.
        print(json.dumps({'observation': 'capture_failed', 'error': type(error).__name__}))
        raise SystemExit(1)
