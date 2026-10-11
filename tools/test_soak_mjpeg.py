# SPDX-License-Identifier: Apache-2.0
import unittest

from soak_mjpeg import JpegMarkers


class MarkerTests(unittest.TestCase):
    def test_complete_and_partial(self):
        markers = JpegMarkers()
        self.assertEqual(markers.feed(b"header\xff\xd8jpeg\xff\xd9\xff\xd8partial"), 1)
        self.assertEqual(markers.feed(b"rest\xff\xd9"), 1)

    def test_every_split(self):
        data = b"--frame\r\n\xff\xd8one\xff\xd9\r\n--frame\r\n\xff\xd8two\xff\xd9"
        for split in range(len(data) + 1):
            markers = JpegMarkers()
            self.assertEqual(markers.feed(data[:split]) + markers.feed(data[split:]), 2)

    def test_byte_at_a_time(self):
        markers = JpegMarkers()
        self.assertEqual(sum(markers.feed(bytes([b])) for b in b"\xff\xd8abc\xff\xd9"), 1)

    def test_reconnect_discards_partial(self):
        self.assertEqual(JpegMarkers().feed(b"\xff\xd8partial"), 0)
        self.assertEqual(JpegMarkers().feed(b"\xff\xd9"), 0)


if __name__ == "__main__":
    unittest.main()
