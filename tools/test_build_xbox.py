# SPDX-License-Identifier: Apache-2.0
import unittest
from build_xbox import merge_settings, parse_env


class EnvTests(unittest.TestCase):
    def test_optional(self):
        self.assertEqual(parse_env(""), {"WIFI_SSID": "", "WIFI_PASSWORD": ""})

    def test_literal_shell_and_special_characters(self):
        values = parse_env('WIFI_SSID="Lab #1"\nWIFI_PASSWORD=\'$(echo nope)$literal\'\n')
        self.assertEqual(values["WIFI_PASSWORD"], "$(echo nope)$literal")
        self.assertEqual(values["WIFI_SSID"], "Lab #1")

    def test_json_round_trip(self):
        import json
        password = 'q"u\\oted #$123'
        values = parse_env('WIFI_SSID="test"\nWIFI_PASSWORD=' + json.dumps(password))
        self.assertEqual(values["WIFI_PASSWORD"], password)
        self.assertIn('CONFIG_KVM_WIFI_DEFAULT_PASSWORD=' + json.dumps(password),
                      merge_settings('', values))

    def test_replaces_stale_defaults(self):
        old = 'CONFIG_KEEP=y\nCONFIG_KVM_WIFI_DEFAULT_SSID="old"\nCONFIG_KVM_WIFI_DEFAULT_PASSWORD="oldpass1"\n'
        result = merge_settings(old, parse_env(''))
        self.assertIn('CONFIG_KEEP=y', result)
        self.assertNotIn('old', result)
        self.assertEqual(result.count('CONFIG_KVM_WIFI_DEFAULT_PASSWORD='), 1)

    def test_invalid_values(self):
        for text in ['WIFI_SSID=a\nWIFI_SSID=b', 'UNKNOWN=x', 'WIFI_PASSWORD=12345678',
                     'WIFI_SSID=test\nWIFI_PASSWORD=short', 'WIFI_SSID="bad\\nline"',
                     'WIFI_SSID="unterminated', 'WIFI_SSID=' + 'x'*33]:
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_env(text)


if __name__ == '__main__':
    unittest.main()
