#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 ESP-KVM contributors
# SPDX-License-Identifier: Apache-2.0
"""Record a bounded microSD clip, validate its decoded tracks, and save local evidence.

Uses ffmpeg/ffprobe. Run with a known audible HDMI source. The output directory
must be new and outside the checkout or ignored by Git. This changes only three
video settings, plus aud_enable for --video-only, and restores those settings.
Do not operate the recorder concurrently: its API has no atomic ownership token.
"""
import argparse
import hashlib
import http.cookiejar
import json
import math
from pathlib import Path, PurePosixPath
import shutil
import ssl
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
VIDEO_SETTINGS = {'vid_codec': 1, 'vid_fps_max': 30, 'h264_kbps': 4000}
RECORD_FIELDS = ('on', 'file', 'seconds', 'bytes', 'frames', 'dropped', 'stopped',
                 'dashcam', 'audio', 'audioFrames', 'audioDropped', 'audioSilenceFrames')


class CheckFailed(Exception):
    """A controlled, non-secret diagnostic suitable for the evidence report."""
    def __init__(self, message, details=None):
        super().__init__(message)
        self.details = details or {}


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


class Device:
    def __init__(self, args):
        self.origin = args.url.rstrip('/')
        cookies = http.cookiejar.MozillaCookieJar()
        if args.cookies:
            cookies.load(str(args.cookies), ignore_discard=True)
        self.opener = urllib.request.build_opener(
            urllib.request.ProxyHandler({}), NoRedirect(),
            urllib.request.HTTPCookieProcessor(cookies),
            urllib.request.HTTPSHandler(context=ssl.create_default_context(cafile=args.ca)))

    def api(self, path, method='GET', body=None):
        request = urllib.request.Request(
            self.origin + path, method=method,
            data=None if body is None else json.dumps(body).encode(),
            headers={'Content-Type': 'application/json', 'X-ESP-KVM': '1'})
        with self.opener.open(request, timeout=5) as response:
            value = json.load(response)
        if not isinstance(value, dict):
            raise CheckFailed('Unexpected API response shape')
        return value

    def download(self, path, destination, seconds):
        url = self.origin + '/api/v1/captures/file?' + urllib.parse.urlencode({'path': path})
        deadline = time.monotonic() + max(60, seconds * 3)
        count = 0
        digest = hashlib.sha256()
        with self.opener.open(url, timeout=15) as response, destination.open('xb') as out:
            expected = response.headers.get('Content-Length')
            while True:
                chunk = response.read(65536)
                if not chunk:
                    break
                count += len(chunk)
                if count > 256 * 1024 * 1024 or time.monotonic() > deadline:
                    raise CheckFailed('Clip download exceeded its size/time bound')
                out.write(chunk)
                digest.update(chunk)
            if expected is not None and count != int(expected):
                raise CheckFailed('Clip download was incomplete')
        if not count:
            raise CheckFailed('Downloaded clip is empty')
        return {'bytes': count, 'sha256': digest.hexdigest()}


def safe_record(record):
    return {key: record[key] for key in RECORD_FIELDS if key in record}


def record_path(value):
    if not isinstance(value, str):
        raise CheckFailed('Recorder returned no clip path')
    path = PurePosixPath(value)
    if (not value.startswith('VIDEO/') or '..' in path.parts or '\\' in value
            or path.suffix not in ('.ts', '.mp4') or str(path) != value):
        raise CheckFailed('Recorder returned an unexpected clip path')
    return value


def stop_owned(device, owned_path):
    """Never issue stop for an unrelated recording or dashcam session."""
    current = device.api('/api/v1/record/status')
    if current.get('on') and not current.get('dashcam') and current.get('file') == owned_path:
        return device.api('/api/v1/record/stop', 'POST', {})
    return current


def restore_settings(device, original):
    allowed = set(VIDEO_SETTINGS) | {'aud_enable'}
    if not original or not set(original) <= allowed:
        raise CheckFailed('Refusing to restore an unexpected settings set')
    device.api('/api/v1/settings', 'PUT', original)
    restored = device.api('/api/v1/settings')
    return all(restored.get(key) == value for key, value in original.items())


def command(*args, timeout=60):
    result = subprocess.run([str(arg) for arg in args], capture_output=True, timeout=timeout)
    if result.returncode:
        # Decoder stderr may contain source filenames; keep the report bounded.
        raise CheckFailed(f'{Path(str(args[0])).name} failed (exit {result.returncode})')
    return result.stdout


def decoded_levels(data, channels):
    if channels < 1 or len(data) % (4 * channels):
        raise CheckFailed('Decoded PCM has an incomplete sample frame')
    peak = energy = 0.0
    nonzero = active = 0
    samples = len(data) // 4
    for (sample,) in struct.iter_unpack('<f', data):
        if not math.isfinite(sample):
            raise CheckFailed('Decoded audio contains non-finite samples')
        peak = max(peak, abs(sample))
        energy += sample * sample
        nonzero += sample != 0
        active += abs(sample) > 32 / 32768
    return {'frames': samples // channels, 'peak': peak,
            'rms': math.sqrt(energy / samples) if samples else 0,
            'nonzero_samples': nonzero, 'active_samples': active}


def validate_clip(clip, expect_audio, seconds):
    info = json.loads(command(
        'ffprobe', '-v', 'error', '-show_entries',
        'stream=index,codec_type,codec_name,profile,sample_rate,channels,width,height,start_time,duration,time_base,nb_frames:'
        'format=duration,size,start_time', '-of', 'json', clip))
    streams = info.get('streams', [])
    video = [stream for stream in streams if stream.get('codec_type') == 'video']
    audio = [stream for stream in streams if stream.get('codec_type') == 'audio']
    if len(video) != 1 or video[0].get('codec_name') != 'h264':
        raise CheckFailed('Expected one H.264 video track')
    if len(audio) != (1 if expect_audio else 0):
        raise CheckFailed('Recording has an unexpected number of audio tracks')
    duration = float(info.get('format', {}).get('duration', 0))
    if not math.isfinite(duration) or not 0 < duration <= seconds + 10:
        raise CheckFailed('Recording duration is empty or exceeds the bounded test')
    # Decode both tracks, with errors fatal, rather than trusting container tags.
    command('ffmpeg', '-nostdin', '-v', 'error', '-xerror', '-i', clip,
            '-map', '0:v:0', '-map', '0:a?', '-t', seconds + 10, '-f', 'null', '-')
    result = {'ffprobe': info, 'audio_tracks': len(audio), 'decode_passed': True,
              'requested_seconds': seconds, 'clip_seconds': duration}
    if expect_audio:
        a = audio[0]
        if a.get('codec_name') != 'aac' or a.get('profile') != 'LC' or a.get('channels') != 2:
            raise CheckFailed('Expected stereo AAC-LC audio')
        pcm = command('ffmpeg', '-nostdin', '-v', 'error', '-xerror', '-i', clip,
                      '-map', '0:a:0', '-t', seconds + 10, '-c:a', 'pcm_f32le', '-f', 'f32le', '-')
        levels = decoded_levels(pcm, a['channels'])
        result['decoded_audio'] = dict(levels, rate=int(a['sample_rate']),
                                       seconds=levels['frames'] / int(a['sample_rate']))
        if not levels['frames'] or not levels['active_samples']:
            raise CheckFailed('Decoded audio has no samples above the activity threshold', result)
        if 'start_time' in a and 'start_time' in video[0]:
            result['audio_minus_video_start_seconds'] = float(a['start_time']) - float(video[0]['start_time'])
        packets = json.loads(command('ffprobe', '-v', 'error', '-select_streams', 'a:0',
                                    '-show_entries', 'packet=pts_time,duration_time', '-of', 'json', clip))['packets']
        gaps = []
        for previous, following in zip(packets, packets[1:]):
            if all(key in previous for key in ('pts_time', 'duration_time')) and 'pts_time' in following:
                gaps.append(float(following['pts_time']) - float(previous['pts_time']) - float(previous['duration_time']))
        result['audio_timing'] = {'packets': len(packets),
                                  'max_gap_seconds': max([0.0] + gaps),
                                  'max_overlap_seconds': max([0.0] + [-gap for gap in gaps])}
    return result


def error_summary(error):
    if isinstance(error, CheckFailed):
        return str(error)
    if isinstance(error, urllib.error.HTTPError):
        return f'HTTP {error.code}'
    return type(error).__name__


def prepare_output(path):
    path = path.resolve()
    if path.is_relative_to(ROOT):
        ignored = subprocess.run(['git', 'check-ignore', '--quiet', '--', str(path)], cwd=ROOT)
        if ignored.returncode:
            raise CheckFailed('Evidence directory inside the checkout must be Git-ignored')
    path.mkdir(parents=True, exist_ok=False)
    return path


def check(args):
    evidence = prepare_output(args.output_dir)
    report = {'test': 'microSD audio-track check', 'requested_seconds': args.seconds,
              'expect_audio': args.expect_audio, 'video_only': args.video_only,
              'network_audio_requested': not args.no_network_audio,
              'date': time.strftime('%Y-%m-%d'), 'passed': False}
    device = Device(args)
    original = {}
    changed = False
    owned_path = None
    observer = None
    saved = None
    failure = None
    try:
        before = device.api('/api/v1/record/status')
        if before.get('on') or before.get('dashcam'):
            raise CheckFailed('Recorder or dashcam is already in use; no test started')
        listener = device.api('/api/v1/audio/status')
        if listener.get('streaming') and (not args.no_network_audio or args.video_only):
            raise CheckFailed('Audio listener is in use; use --no-network-audio for a normal audio recording')
        settings = device.api('/api/v1/settings')
        original = {key: settings[key] for key in VIDEO_SETTINGS}
        desired = dict(VIDEO_SETTINGS)
        if args.video_only:
            original['aud_enable'] = settings['aud_enable']
            desired['aud_enable'] = False
        elif args.expect_audio and not settings.get('aud_enable'):
            raise CheckFailed('Enable HDMI audio before running an audio recording check')
        # Persist only these allowlisted values, never the complete settings response.
        with (evidence / 'original-settings.json').open('x') as out:
            json.dump(original, out, indent=2)
            out.write('\n')
        report['original_settings'] = original
        changed = True  # A failed PUT may still have reached the device.
        device.api('/api/v1/settings', 'PUT', desired)
        time.sleep(1)
        for attempt in range(5):
            current = device.api('/api/v1/record/status')
            if current.get('on') or current.get('dashcam'):
                raise CheckFailed('Recorder became active before our start; leaving it alone')
            try:
                started = device.api(f'/api/v1/record/start?seconds={args.seconds}', 'POST', {})
                owned_path = record_path(started.get('file'))
                if owned_path == before.get('file'):
                    raise CheckFailed('Recorder did not return a unique new clip path')
                break
            except urllib.error.HTTPError as error:
                if error.code != 409 or attempt == 4:
                    raise
                time.sleep(1)
        print('Started bounded microSD clip:', owned_path, flush=True)
        start_time = time.monotonic()
        if not args.no_network_audio:
            argv = [sys.executable, str(ROOT / 'tools/observe_audio.py'), args.url,
                    '--seconds', str(args.seconds), '--output', str(evidence / 'source-audio.wav')]
            if args.cookies:
                argv += ['--cookies', str(args.cookies)]
            if args.ca:
                argv += ['--ca', args.ca]
            observer = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        while time.monotonic() - start_time < args.seconds + 10:
            current = device.api('/api/v1/record/status')
            if current.get('file') != owned_path or current.get('dashcam'):
                raise CheckFailed('Recorder ownership changed; leaving the current recording alone')
            if not current.get('on'):
                saved = current
                break
            time.sleep(.5)
        if saved is None:
            saved = stop_owned(device, owned_path)
        if saved.get('file') != owned_path or saved.get('on') or not saved.get('bytes'):
            raise CheckFailed('The owned recording did not finish with saved data')
        report['record'] = safe_record(saved)
        if observer is not None:
            stdout, _ = observer.communicate(timeout=5)
            report['parallel_audio_exit'] = observer.returncode
            observed = json.loads(stdout)
            allowed = ('frames', 'rate', 'audio_seconds', 'wall_seconds', 'peak', 'rms', 'active_frames', 'observation', 'error')
            report['parallel_audio_capture'] = {key: observed[key] for key in allowed if key in observed}
            if observer.returncode or not observed.get('frames'):
                raise CheckFailed('Parallel network audio capture failed')
    except (Exception, KeyboardInterrupt) as error:
        failure = error
    finally:
        if observer is not None and observer.poll() is None:
            observer.terminate()
            try:
                observer.communicate(timeout=3)
            except subprocess.TimeoutExpired:
                observer.kill()
                observer.communicate()
        if owned_path is not None:
            try:
                current = stop_owned(device, owned_path)
                report['owned_recording_stopped'] = not (current.get('on') and current.get('file') == owned_path)
            except Exception as error:
                report['stop_verification_error'] = error_summary(error)
                failure = failure or error
        if changed:
            try:
                report['settings_restored'] = restore_settings(device, original)
                if not report['settings_restored']:
                    failure = failure or CheckFailed('Changed settings did not restore correctly')
            except Exception as error:
                report['settings_restored'] = False
                report['restore_error'] = error_summary(error)
                failure = failure or error
        print('Settings restored:', report.get('settings_restored', 'not changed'), flush=True)
    try:
        if saved is None or saved.get('file') != owned_path or saved.get('on') or not saved.get('bytes'):
            raise failure or CheckFailed('No completed owned clip is available')
        clip = evidence / ('card-recording' + PurePosixPath(owned_path).suffix)
        report['download'] = device.download(owned_path, clip, args.seconds)
        report.update(validate_clip(clip, args.expect_audio, args.seconds))
        report['passed'] = failure is None
        if failure is not None:
            report['error'] = error_summary(failure)
    except (Exception, KeyboardInterrupt) as error:
        if isinstance(error, CheckFailed):
            report.update(error.details)
        report['error'] = error_summary(error)
    with (evidence / 'result.json').open('x') as out:
        json.dump(report, out, indent=2)
        out.write('\n')
    print(json.dumps(report, indent=2))
    print('Local evidence:', evidence)
    return 0 if report['passed'] else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('url', help='Device HTTP(S) origin without credentials')
    parser.add_argument('--seconds', type=int, default=8, help='Bounded recording duration, 1..60 (default: 8)')
    parser.add_argument('--output-dir', required=True, type=Path, help='New ignored/outside-checkout evidence directory')
    parser.add_argument('--no-network-audio', action='store_true', help='Do not open a parallel live-audio listener')
    parser.add_argument('--expect-audio', action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument('--video-only', action='store_true', help='Temporarily disable HDMI audio and require a video-only clip')
    parser.add_argument('--cookies', type=Path, help='Existing Netscape session-cookie file')
    parser.add_argument('--ca', help='Device TLS certificate file')
    args = parser.parse_args(argv)
    url = urllib.parse.urlsplit(args.url)
    if (url.scheme not in ('http', 'https') or not url.hostname or url.username or url.password
            or url.query or url.fragment or url.path not in ('', '/')):
        parser.error('URL must be an HTTP(S) origin without credentials, query or path')
    if not 1 <= args.seconds <= 60:
        parser.error('--seconds must be in 1..60')
    if args.video_only:
        args.expect_audio = False
        args.no_network_audio = True
    if not args.expect_audio:
        args.no_network_audio = True
    if not shutil.which('ffmpeg') or not shutil.which('ffprobe'):
        parser.error('ffmpeg and ffprobe are required')
    try:
        return check(args)
    except (Exception, KeyboardInterrupt) as error:
        print(json.dumps({'passed': False, 'error': error_summary(error)}))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
