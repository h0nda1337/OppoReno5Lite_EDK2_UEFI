#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Collect private hardware evidence without rebooting or writing to a phone."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile
import time

from device_session import BatteryBelowMinimum, ReadOnlyAdb

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--private-dir', required=True, type=Path)
    args = parser.parse_args()
    private = args.private_dir.resolve()
    if private.is_relative_to(ROOT):
        parser.error('Private capture directory must be outside the public repository')
    private.mkdir(parents=True, exist_ok=True)
    if (private / 'capture.json').exists():
        parser.error('Choose a fresh capture directory to preserve earlier evidence')
    session = ReadOnlyAdb(args.serial)
    record = {'state': 'collecting', 'collected_at': time.time(),
              'phone_write_performed': False, 'phone_reboot_performed': False,
              'files': {}, 'read_errors': {}}
    try:
        record['properties'] = session.properties()
        tree = session.device_tree_tar()
        with tarfile.open(fileobj=io.BytesIO(tree)) as archive:
            root_properties = [m for m in archive.getmembers() if m.isfile()
                               and m.name.removeprefix('./') in ('compatible', 'model')
                               and m.size > 0]
            if not root_properties:
                raise RuntimeError('No readable root properties in device tree archive')
        (private / 'device-tree.tar').write_bytes(tree)
        record['files']['device-tree.tar'] = {
            'size': len(tree), 'sha256': hashlib.sha256(tree).hexdigest()}
        paths = {
            'iomem.txt': '/proc/iomem',
            'fb-name.txt': '/sys/class/graphics/fb0/name',
            'fb-size.txt': '/sys/class/graphics/fb0/virtual_size',
            'fb-bpp.txt': '/sys/class/graphics/fb0/bits_per_pixel',
            'fb-stride.txt': '/sys/class/graphics/fb0/stride',
        }
        for name, path in paths.items():
            try:
                data = session.read_file(path)
            except BatteryBelowMinimum:
                raise
            except RuntimeError as error:
                record['read_errors'][name] = str(error)
                continue
            (private / name).write_bytes(data)
            record['files'][name] = {'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        session.battery()
        record['state'] = 'complete'
    except BaseException as error:
        record['state'] = 'stopped'
        record['error'] = str(error)
        raise
    finally:
        record['battery_samples'] = session.battery_samples
        (private / 'capture.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
        print(json.dumps({'state': record['state'], 'files': list(record['files']),
                          'read_errors': list(record['read_errors']),
                          'battery_samples': session.battery_samples}, indent=2))


if __name__ == '__main__':
    main()
