#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Read-only ADB session with explicit target and a mandatory battery floor."""
import re
import shutil
import subprocess

MIN_BATTERY_PERCENT = 20
PUBLIC_PROPERTIES = (
    'ro.product.model', 'ro.product.device', 'ro.board.platform',
    'ro.boot.prjname', 'ro.hardware', 'ro.build.version.ota',
    'ro.boot.flash.locked', 'ro.boot.vbmeta.device_state',
    'ro.build.oplus_nv_id',
)


class BatteryBelowMinimum(RuntimeError):
    pass


def parse_properties(text):
    selected = {}
    for line in text.splitlines():
        match = re.fullmatch(r'\[([^\]]+)\]: \[(.*)\]', line)
        if match and match.group(1) in PUBLIC_PROPERTIES:
            selected[match.group(1)] = match.group(2)
    return {key: selected.get(key, '') for key in PUBLIC_PROPERTIES}


def parse_battery(text):
    def field(name):
        matches = re.findall(r'^\s*' + re.escape(name) + r':\s*(\S+)\s*$', text, re.M)
        if not matches:
            raise ValueError('Battery field unavailable: ' + name)
        return matches[-1]
    level, scale = int(field('level')), int(field('scale'))
    if not 0 <= level <= scale or scale <= 0:
        raise ValueError('Invalid battery level/scale')
    return {'percent': 100 * level / scale,
            'usb_powered': field('USB powered') == 'true',
            'status': int(field('status')),
            'minimum_percent': MIN_BATTERY_PERCENT}


class ReadOnlyAdb:
    def __init__(self, serial):
        if not serial or serial.startswith('-'):
            raise ValueError('An explicit device serial is required')
        self.serial = serial
        self.adb = shutil.which('adb')
        if not self.adb:
            raise RuntimeError('ADB is unavailable')
        self.battery_samples = []

    def _run(self, *args, timeout=30):
        result = subprocess.run([self.adb, '-s', self.serial, *args],
                                capture_output=True, timeout=timeout)
        if result.returncode:
            error = result.stderr.decode('utf-8', 'replace').strip()
            lines = error.splitlines()
            summary = '\n'.join(lines[:3])[:1000]
            if len(lines) > 3:
                summary += '\n[additional remote errors omitted]'
            raise RuntimeError(summary or 'ADB read failed')
        return result.stdout

    def battery(self):
        sample = parse_battery(self._run('shell', 'dumpsys', 'battery').decode('utf-8'))
        self.battery_samples.append(sample)
        if sample['percent'] < MIN_BATTERY_PERCENT:
            raise BatteryBelowMinimum('Battery is below the required 20%; phone operations stopped')
        return sample

    def properties(self):
        self.battery()
        text = self._run('shell', 'getprop').decode('utf-8', 'replace')
        selected = parse_properties(text)
        expected = {'ro.product.model': 'CPH2205', 'ro.product.device': 'OP4F43L1',
                    'ro.board.platform': 'mt6779', 'ro.boot.prjname': '20295'}
        if any(selected[key] != value for key, value in expected.items()):
            raise RuntimeError('Connected target does not match the OPPO project')
        return selected

    def read_file(self, path):
        self.battery()
        return self._run('shell', '-T', 'cat', path)

    def device_tree_tar(self):
        self.battery()
        return self._run('shell', '-T', 'tar', '-C', '/sys/firmware/devicetree/base',
                         '-cf', '-', '.', timeout=60)
