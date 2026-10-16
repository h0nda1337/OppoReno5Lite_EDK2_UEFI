#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Filter hardware ranges and framebuffer handoff from a PRIVATE runtime DTB.

The resulting inventory excludes bootargs, identifiers and arbitrary /chosen
properties. Parsing a supplied DTB does not prove that EDK2 executed on hardware.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from inspect_platform import inventory, parse_fdt, u32


def memory_range(base, size):
    if not 0 <= base < 2**64 or not 0 < size < 2**64 or base + size >= 2**64:
        raise ValueError('Invalid or overflowing physical range')
    return {'address': hex(base), 'size': hex(size)}


def reservations(raw):
    total, position = u32(raw, 4), u32(raw, 16)
    if position < 40 or position % 8:
        raise ValueError('Invalid FDT memory reservation table offset')
    structure = (u32(raw, 8), u32(raw, 8) + u32(raw, 36))
    strings = (u32(raw, 12), u32(raw, 12) + u32(raw, 32))
    result = []
    for _ in range(256):
        if position + 16 > total or any(position < end and start < position + 16
                                        for start, end in (structure, strings)):
            raise ValueError('Truncated or overlapping FDT memory reservation table')
        base, size = struct.unpack_from('>QQ', raw, position)
        if base == size == 0:
            return result
        result.append(memory_range(base, size))
        position += 16
    raise ValueError('Excessive or unterminated FDT memory reservation table')


def framebuffer_tag(chosen):
    results = []
    raw = chosen.get('atag,videolfb')
    if raw is not None:
        # OPPO's MT6779 kernel consumes a native little-endian tag_videolfb,
        # unlike the big-endian cells used by ordinary FDT properties.
        if len(raw) < 20:
            raise ValueError('Truncated MediaTek videolfb tag')
        base, found, fps, size = struct.unpack_from('<QIII', raw)
        results.append(dict(memory_range(base, size), lcm_found=bool(found), fps_raw=fps,
                            encoding='native-little-endian-struct'))
    prefix = 'atag,videolfb-'
    keys = [prefix + name for name in ('fb_base_h', 'fb_base_l', 'vramSize')]
    if any(key in chosen for key in keys):
        if any(key not in chosen or len(chosen[key]) != 4 for key in keys):
            raise ValueError('Incomplete MediaTek split videolfb properties')
        high, low, size = (u32(chosen[key]) for key in keys)
        results.append(dict(memory_range(high << 32 | low, size), encoding='big-endian-cells'))
    if len(results) > 1 and any(results[0][key] != results[1][key] for key in ('address', 'size')):
        raise ValueError('Conflicting MediaTek framebuffer handoffs')
    return results


def inspect(raw):
    nodes = parse_fdt(raw)
    hardware = inventory(nodes)
    if not hardware['memory_nodes']:
        raise ValueError('No runtime memory nodes; the static stock DTB is insufficient')
    for node in hardware['memory_nodes']:
        if not node['reg']:
            raise ValueError('Empty memory node')
        for item in node['reg']:
            memory_range(int(item['address'], 16), int(item['size'], 16))
    chosen = nodes.get('/chosen', nodes.get('/chosen@0', {}))
    initrd = None
    if 'linux,initrd-start' in chosen or 'linux,initrd-end' in chosen:
        values = [chosen.get(key, b'') for key in ('linux,initrd-start', 'linux,initrd-end')]
        if any(len(value) not in (4, 8) for value in values):
            raise ValueError('Invalid initrd handoff')
        start, end = (int.from_bytes(value, 'big') for value in values)
        initrd = memory_range(start, end - start)
    return {'schema_version': 1, 'source': 'supplied-runtime-dtb',
            'source_dtb_sha256': hashlib.sha256(raw).hexdigest(),
            'hardware': hardware, 'reservation_table': reservations(raw),
            'initrd': initrd, 'framebuffer_handoffs': framebuffer_tag(chosen),
            'uefi_execution_observed': False, 'phone_operations_performed': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('private_runtime_dtb', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.private_runtime_dtb.resolve() == args.output.resolve():
        parser.error('Output must not overwrite the private DTB')
    report = inspect(args.private_runtime_dtb.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Filtered runtime hardware inventory written; no phone operation performed.')


if __name__ == '__main__':
    main()
