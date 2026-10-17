# SPDX-License-Identifier: Apache-2.0
import struct
import unittest
from observe_audio import levels


class AudioEvidenceTest(unittest.TestCase):
    def test_silence(self):
        self.assertEqual(levels(bytes(40)), {'peak': 0, 'rms': 0, 'active_frames': 0})

    def test_either_channel_counts_as_sound(self):
        data = struct.pack('<hhhh', -32768, 0, 0, 16384)
        result = levels(data)
        self.assertEqual(result['active_frames'], 2)
        self.assertEqual(result['peak'], 1)
        self.assertAlmostEqual(result['rms'], (1.25 / 4) ** 0.5)

    def test_small_noise_is_not_counted_as_activity(self):
        self.assertEqual(levels(struct.pack('<hhhh', 32, -32, 0, 1))['active_frames'], 0)


if __name__ == '__main__':
    unittest.main()
