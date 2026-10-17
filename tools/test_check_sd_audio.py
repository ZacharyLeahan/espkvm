#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 ESP-KVM contributors
# SPDX-License-Identifier: Apache-2.0
"""Pure host checks for evidence validation and recorder/settings ownership."""
import argparse
import contextlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import Mock, patch

import check_sd_audio as check


class AudioCheckTests(unittest.TestCase):
    def test_stop_only_owned_recording(self):
        for record in ({'on': True, 'file': 'VIDEO/other.ts'},
                       {'on': True, 'file': 'VIDEO/ours.ts', 'dashcam': True},
                       {'on': False, 'file': 'VIDEO/ours.ts'}):
            device = Mock()
            device.api.return_value = record
            self.assertEqual(check.stop_owned(device, 'VIDEO/ours.ts'), record)
            device.api.assert_called_once_with('/api/v1/record/status')
        device = Mock()
        device.api.side_effect = [{'on': True, 'file': 'VIDEO/ours.ts'}, {'on': False}]
        self.assertEqual(check.stop_owned(device, 'VIDEO/ours.ts'), {'on': False})
        self.assertEqual(device.api.call_args.args, ('/api/v1/record/stop', 'POST', {}))

    def test_restore_allows_only_test_settings(self):
        values = {'vid_codec': 0, 'vid_fps_max': 6, 'h264_kbps': 1200}
        device = Mock()
        device.api.side_effect = [{}, dict(values, wifi_password='private')]
        self.assertTrue(check.restore_settings(device, values))
        self.assertEqual(device.api.call_args_list[0].args, ('/api/v1/settings', 'PUT', values))
        with self.assertRaises(check.CheckFailed):
            check.restore_settings(device, {'wifi_password': 'private'})

    def test_pcm_validation_and_levels(self):
        levels = check.decoded_levels(struct.pack('<ffff', 0, .5, -1, 0), 2)
        self.assertEqual(levels['frames'], 2)
        self.assertEqual(levels['peak'], 1)
        self.assertEqual(levels['nonzero_samples'], 2)
        self.assertAlmostEqual(levels['rms'], (1.25 / 4) ** .5)
        for payload in (b'bad', struct.pack('<ff', float('nan'), 0)):
            with self.assertRaises(check.CheckFailed):
                check.decoded_levels(payload, 2)

    def test_path_is_scoped_to_returned_video_file(self):
        self.assertEqual(check.record_path('VIDEO/test.ts'), 'VIDEO/test.ts')
        for value in ('../secret', '/VIDEO/test.ts', 'VIDEO/../secret.ts', 'VIDEO//test.ts', None):
            with self.assertRaises(check.CheckFailed):
                check.record_path(value)

    def test_audio_must_decode_nonzero_samples(self):
        info = {'format': {'duration': '8'}, 'streams': [
            {'codec_type': 'video', 'codec_name': 'h264', 'start_time': '.7'},
            {'codec_type': 'audio', 'codec_name': 'aac', 'profile': 'LC', 'channels': 2,
             'sample_rate': '48000', 'start_time': '.8'}]}
        packets = {'packets': [{'pts_time': '.8', 'duration_time': '.02'},
                               {'pts_time': '.84', 'duration_time': '.02'}]}
        with patch.object(check, 'command', side_effect=[json.dumps(info).encode(), b'',
                          struct.pack('<ffff', 0, .5, -.5, 0), json.dumps(packets).encode()]) as command:
            result = check.validate_clip(Path('test.ts'), True, 8)
            self.assertTrue(result['decode_passed'])
            self.assertEqual(result['decoded_audio']['frames'], 2)
            self.assertAlmostEqual(result['audio_minus_video_start_seconds'], .1)
            self.assertAlmostEqual(result['audio_timing']['max_gap_seconds'], .02)
            self.assertIn('-xerror', command.call_args_list[1].args)
        with patch.object(check, 'command', side_effect=[json.dumps(info).encode(), b'', b'\0' * 16]):
            with self.assertRaises(check.CheckFailed) as failure:
                check.validate_clip(Path('test.ts'), True, 8)
            self.assertEqual(failure.exception.details['decoded_audio']['nonzero_samples'], 0)

    def test_partial_settings_failure_restores_without_exposing_secrets(self):
        class Device:
            def __init__(self):
                self.puts = []
            def api(self, path, method='GET', body=None):
                if path == '/api/v1/record/status':
                    return {'on': False, 'dashcam': False}
                if path == '/api/v1/audio/status':
                    return {'streaming': False}
                if method == 'PUT':
                    self.puts.append(body)
                    if len(self.puts) == 1:
                        raise OSError('this diagnostic may contain secrets')
                    return {}
                return {'vid_codec': 0, 'vid_fps_max': 6, 'h264_kbps': 1200,
                        'aud_enable': True, 'wifi_password': 'NEVER-PRINT-THIS'}
        device = Device()
        args = argparse.Namespace(seconds=8, expect_audio=True, video_only=False,
                                  no_network_audio=True, output_dir=None)
        with tempfile.TemporaryDirectory() as temp:
            captured = io.StringIO()
            with patch.object(check, 'prepare_output', return_value=Path(temp)), \
                 patch.object(check, 'Device', return_value=device), contextlib.redirect_stdout(captured):
                self.assertEqual(check.check(args), 1)
            report = json.loads((Path(temp) / 'result.json').read_text())
            self.assertTrue(report['settings_restored'])
            self.assertEqual(set(device.puts[-1]), set(check.VIDEO_SETTINGS))
            self.assertNotIn('NEVER-PRINT-THIS', captured.getvalue())
            self.assertNotIn('this diagnostic may contain secrets', captured.getvalue())


if __name__ == '__main__':
    unittest.main()
