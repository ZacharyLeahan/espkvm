# SPDX-License-Identifier: Apache-2.0
import unittest
from observe_xbox import observation, transition


class ObservationTests(unittest.TestCase):
    def test_lock_does_not_prove_freshness(self):
        self.assertEqual(observation({'signal': True, 'fps': 0})['capture_freshness'], 'unknown')

    def test_no_signal(self):
        self.assertEqual(observation({'signal': False})['capture_freshness'], 'no_signal')

    def test_private_fields_not_copied(self):
        self.assertNotIn('secret', observation({'signal': True, 'secret': 'private'})['video'])

    def test_missing_signal_is_not_success(self):
        for value in ({}, [], {'signal': 'false'}):
            with self.assertRaises(ValueError):
                observation(value)

    def test_transition(self):
        self.assertEqual(transition({'signal': True}, {'signal': True}), {})
        self.assertEqual(transition({'height': 480}, {'height': 720}),
                         {'height': {'from': 480, 'to': 720}})


if __name__ == '__main__':
    unittest.main()
