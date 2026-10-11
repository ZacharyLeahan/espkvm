#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Read-only Xbox test evidence using existing ESP-KVM APIs; no controller input."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import time
import urllib.error
import urllib.parse
import urllib.request

VIDEO_FIELDS = ('signal', 'width', 'height', 'interlaced', 'inputHz', 'tooFast',
                'fps', 'skippedFps', 'modeChanges', 'viewers', 'codec')


def observation(status):
    # HDMI lock and encoded FPS cannot establish CSI freshness or game health.
    if not isinstance(status, dict) or not isinstance(status.get('signal'), bool):
        raise ValueError('Missing boolean HDMI signal status')
    return {'video': {k: status[k] for k in VIDEO_FIELDS if k in status},
            'capture_freshness': 'unknown' if status['signal'] else 'no_signal'}


def transition(previous, current):
    keys = ('signal', 'width', 'height', 'interlaced', 'inputHz', 'viewers')
    return {k: {'from': previous.get(k), 'to': current.get(k)}
            for k in keys if previous.get(k) != current.get(k)}


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def fetch(opener, url, expected, limit):
    with opener.open(url, timeout=5) as response:
        if response.headers.get_content_type() != expected:
            raise ValueError('Unexpected response content type')
        data = response.read(limit + 1)
        if len(data) > limit:
            raise ValueError('Response exceeds evidence limit')
        return data


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('url', help='Device origin, e.g. http://esp.local')
    parser.add_argument('--build-id', required=True, help='Homebrew commit/build under test, not KVM commit')
    parser.add_argument('--scenario', required=True, help='Human label such as dashboard, 480p-game, reset')
    parser.add_argument('--seconds', type=float, default=10)
    parser.add_argument('--interval', type=float, default=2)
    parser.add_argument('--screenshot', action='store_true', help='One initial JPEG; briefly consumes video resources')
    parser.add_argument('--output', type=Path, required=True, help='NEW private session directory')
    args = parser.parse_args(argv)
    u = urllib.parse.urlsplit(args.url)
    if (u.scheme not in ('http', 'https') or not u.hostname or u.username or u.password
            or u.query or u.fragment or u.path not in ('', '/')):
        parser.error('URL must be an HTTP(S) origin without credentials, query or path')
    if not (0 < args.seconds <= 86400 and 0.2 <= args.interval <= 3600):
        parser.error('Duration must be 0-86400 seconds; interval 0.2-3600 seconds')
    args.output.mkdir(parents=True, exist_ok=False, mode=0o700)
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())
    origin = args.url.rstrip('/')
    start = time.monotonic()
    previous = None
    failures = samples = 0
    with (args.output / 'events.jsonl').open('x', encoding='utf-8') as out:
        def emit(event, **fields):
            row = dict(schema=1, event=event, utc=datetime.now(timezone.utc).isoformat(),
                       elapsed_s=round(time.monotonic() - start, 3), **fields)
            out.write(json.dumps(row) + '\n')
            out.flush()
        emit('session', build_id=args.build_id, scenario=args.scenario,
             screenshot_requested=args.screenshot,
             note='Read-only sampling; no automated pass/fail judgment of game health.')
        try:
            while time.monotonic() - start < args.seconds:
                try:
                    raw = fetch(opener, origin + '/api/v1/video/status', 'application/json', 65536)
                    item = observation(json.loads(raw))
                    emit('observation', **item)
                    if previous is not None:
                        changes = transition(previous, item['video'])
                        if changes:
                            emit('transition_observed', changes=changes)
                    previous = item['video']
                    if args.screenshot and samples == 0:
                        jpeg = fetch(opener, origin + '/api/v1/video/frame.jpg', 'image/jpeg', 4 * 1024 * 1024)
                        if not (jpeg.startswith(b'\xff\xd8') and jpeg.endswith(b'\xff\xd9')):
                            raise ValueError('Incomplete JPEG')
                        (args.output / 'frame.jpg').write_bytes(jpeg)
                        emit('screenshot', file='frame.jpg', sha256=hashlib.sha256(jpeg).hexdigest(),
                             note='Frame endpoint succeeded; does not prove game responsiveness.')
                except (OSError, ValueError) as exc:
                    failures += 1
                    # Avoid saving request URLs, cookies, private server error bodies.
                    emit('request_failed', error=type(exc).__name__,
                         http_status=exc.code if isinstance(exc, urllib.error.HTTPError) else None)
                samples += 1
                time.sleep(min(args.interval, max(0, args.seconds - (time.monotonic() - start))))
        except KeyboardInterrupt:
            emit('interrupted')
            return 130
        emit('finished', samples=samples, request_failures=failures)
    print(json.dumps({'evidence': str(args.output), 'samples': samples, 'request_failures': failures}))
    return 1 if failures else 0


if __name__ == '__main__':
    raise SystemExit(main())
