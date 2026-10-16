#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Validate private Linux observations and export only filtered hardware facts.

No phone operation. An active Linux framebuffer does not prove that GOP works
before Linux initializes the display. Raw FDT, iomem and logs remain private.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

from inspect_runtime import inspect, memory_range


def integer(value, low, high):
    if type(value) is not int or not low <= value <= high:
        raise ValueError('Invalid framebuffer integer')
    return value


def framebuffer_observation(runtime, observation):
    if observation.get('schema_version') != 1 or observation.get('source') != 'linux-fb-ioctl':
        raise ValueError('Unexpected framebuffer observation source')
    base_text = observation.get('physical_base', '')
    if not isinstance(base_text, str) or not re.fullmatch(r'0x[0-9a-fA-F]+', base_text):
        raise ValueError('Invalid framebuffer physical address')
    base = int(base_text, 16)
    size = integer(observation.get('memory_size'), 1, 256 * 1024**2)
    memory_range(base, size)
    width = integer(observation.get('width'), 64, 4096)
    height = integer(observation.get('height'), 128, 8192)
    virtual_width = integer(observation.get('virtual_width'), width, 8192)
    virtual_height = integer(observation.get('virtual_height'), height, 32768)
    stride = integer(observation.get('stride_bytes'), width * 4, 65536)
    xoffset = integer(observation.get('xoffset'), 0, virtual_width - width)
    yoffset = integer(observation.get('yoffset'), 0, virtual_height - height)
    if (observation.get('bits_per_pixel') != 32 or stride % 4 or
            stride < virtual_width * 4 or stride * virtual_height > size):
        raise ValueError('Framebuffer storage geometry is inconsistent')
    channels = {}
    used = 0
    for name in ('red', 'green', 'blue', 'alpha'):
        value = observation.get(name)
        if not isinstance(value, list) or len(value) != 2:
            raise ValueError('Invalid framebuffer channel')
        offset = integer(value[0], 0, 24)
        length = integer(value[1], 0, 8)
        if length != 8 and not (name == 'alpha' and length == 0):
            raise ValueError('Unsupported framebuffer channel width')
        mask = ((1 << length) - 1) << offset
        if used & mask:
            raise ValueError('Overlapping framebuffer channels')
        used |= mask
        channels[name] = [offset, length]
    tags = [item['encoding'] for item in runtime['framebuffer_handoffs']
            if int(item['address'], 16) == base and int(item['size'], 16) >= size]
    reservations = [item['path'] for item in runtime['hardware']['reserved_memory']
                    if 'framebuffer' in item['path'] and any(
                        int(region['address'], 16) <= base and
                        base + size <= int(region['address'], 16) + int(region['size'], 16)
                        for region in item['reg'])]
    if not tags or not reservations:
        raise ValueError('Active framebuffer lacks matching runtime tag and reservation')
    # Reconstruct a whitelist instead of copying arbitrary observation fields.
    return {'schema_version': 1, 'source': 'linux-fb-ioctl-corroborated-by-runtime-fdt',
            'physical_base': hex(base), 'memory_size': size, 'stride_bytes': stride,
            'width': width, 'height': height, 'virtual_width': virtual_width,
            'virtual_height': virtual_height, 'xoffset': xoffset, 'yoffset': yoffset,
            'bits_per_pixel': 32, **channels,
            'visible_physical_base': hex(base + yoffset * stride + xoffset * 4),
            'matching_runtime_tags': tags, 'matching_reservations': reservations,
            'runtime_tags_disagree': not runtime['framebuffer_handoffs_consistent'],
            'linux_driver_framebuffer_verified': True, 'native_gop_execution_verified': False}


def iomem_ranges(text):
    selected = []
    for line in text.splitlines():
        match = re.fullmatch(r'(\s*)([0-9a-fA-F]+)-([0-9a-fA-F]+) : (.*)', line)
        if not match or match[4] not in ('System RAM', 'Kernel code', 'Kernel data', 'reserved'):
            continue
        base, end = int(match[2], 16), int(match[3], 16)
        selected.append(dict(memory_range(base, end - base + 1), kind=match[4],
                             nested=bool(match[1])))
        if len(selected) > 4096:
            raise ValueError('Excessive filtered iomem ranges')
    if not any(item['kind'] == 'System RAM' for item in selected):
        raise ValueError('No usable iomem observations')
    return selected


def analyze(directory):
    names = ('runtime.dtb', 'iomem.txt', 'framebuffer.json', 'battery.txt')
    data = {name: (directory / name).read_bytes() for name in names}
    if any(not value or len(value) > 2 * 1024**2 for value in data.values()):
        raise ValueError('Missing or excessive diagnostic observation')
    level = int(data['battery.txt'].strip())
    if not 20 <= level <= 100:
        raise ValueError('Battery sample outside permitted range')
    runtime = inspect(data['runtime.dtb'])
    if not any('mt6779' in item.lower() for item in runtime['hardware']['compatible']):
        raise ValueError('Not an MT6779 capture')
    screen = framebuffer_observation(runtime, json.loads(data['framebuffer.json']))
    ranges = iomem_ranges(data['iomem.txt'].decode('ascii'))
    report = {'schema_version': 1, 'source': 'private-minimal-linux-diagnostic-capture',
              'battery_percent_during_capture': level,
              'linux_diagnostic_execution_verified': True, 'uefi_execution_observed': False,
              'raw_files_included': False,
              'files': {name: {'size': len(value), 'sha256': hashlib.sha256(value).hexdigest()}
                        for name, value in data.items()},
              'framebuffer': screen, 'runtime': runtime, 'linux_iomem_ranges': ranges,
              'uefi_memory_map_complete': False,
              'limits': ['Linux observations are not a pre-Linux display validation',
                         'Dynamic size-only FDT reservations require separate treatment',
                         'Conflicting framebuffer tags are retained; no native address is selected']}
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--private-capture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve() in [item.resolve() for item in args.private_capture.iterdir()]:
        parser.error('Filtered output must not overwrite private observations')
    report = analyze(args.private_capture)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Filtered Linux hardware observations verified; native UEFI remains unverified.')


if __name__ == '__main__':
    main()
