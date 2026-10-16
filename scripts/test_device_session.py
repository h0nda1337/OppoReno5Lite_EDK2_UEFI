#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Offline checks of the phone-selection and battery boundaries."""
import unittest
from unittest.mock import patch

from device_session import BatteryBelowMinimum, ReadOnlyAdb, parse_battery, parse_properties


def battery(level):
    return f'  USB powered: true\r\n  status: 2\r\n  level: {level}\r\n  scale: 100\r\n'.encode()


class SessionTests(unittest.TestCase):
    def test_exact_minimum_allowed(self):
        with patch('device_session.shutil.which', return_value='adb'):
            session = ReadOnlyAdb('EXAMPLE_TEST_TARGET')
        session._run = lambda *args: battery(20)
        self.assertEqual(session.battery()['percent'], 20)

    def test_low_battery_prevents_following_read(self):
        with patch('device_session.shutil.which', return_value='adb'):
            session = ReadOnlyAdb('EXAMPLE_TEST_TARGET')
        calls = []
        def transport(*args):
            calls.append(args)
            return battery(19)
        session._run = transport
        with self.assertRaises(BatteryBelowMinimum):
            session.read_file('/proc/iomem')
        self.assertEqual(calls, [('shell', 'dumpsys', 'battery')])

    def test_wrong_target_rejected(self):
        with patch('device_session.shutil.which', return_value='adb'):
            session = ReadOnlyAdb('EXAMPLE_TEST_TARGET')
        session._run = lambda *args: battery(21) if args[-1] == 'battery' else b'[ro.product.model]: [Other phone]\n'
        with self.assertRaisesRegex(RuntimeError, 'does not match'):
            session.properties()

    def test_crlf_properties_are_filtered(self):
        props = parse_properties('[ro.product.model]: [CPH2205]\r\n[private.example]: [excluded]\r\n')
        self.assertEqual(props['ro.product.model'], 'CPH2205')
        self.assertNotIn('private.example', props)

    def test_unknown_battery_fails_closed(self):
        with self.assertRaises(ValueError):
            parse_battery('level: 50\n')


if __name__ == '__main__':
    unittest.main()
