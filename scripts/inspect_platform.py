#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Extract a filtered platform inventory from a saved Android v2 boot image."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

FDT_MAGIC = 0xD00DFEED
DT_TABLE_MAGIC = 0xD7B7AB1E


def u32(data, offset=0):
    if offset < 0 or offset + 4 > len(data):
        raise ValueError('Truncated 32-bit field')
    return struct.unpack_from('>I', data, offset)[0]


def parse_fdt(data):
    if len(data) < 40 or u32(data) != FDT_MAGIC:
        raise ValueError('Invalid FDT header')
    total, structs, strings = u32(data, 4), u32(data, 8), u32(data, 12)
    strings_size, structs_size = u32(data, 32), u32(data, 36)
    if total > len(data) or structs + structs_size > total or strings + strings_size > total:
        raise ValueError('Invalid FDT block bounds')
    pos, end = structs, structs + structs_size
    stack, nodes = [], {}
    while pos < end:
        token = u32(data, pos)
        pos += 4
        if token == 1:
            name_end = data.find(b'\0', pos, end)
            if name_end < 0:
                raise ValueError('Unterminated node name')
            name = data[pos:name_end].decode('ascii')
            stack.append(name)
            nodes['/' + '/'.join(stack[1:])] = {}
            pos = (name_end + 4) & ~3
        elif token == 2:
            if not stack:
                raise ValueError('Unbalanced node end')
            stack.pop()
        elif token == 3:
            length, name_offset = u32(data, pos), u32(data, pos + 4)
            pos += 8
            if not stack or pos + length > end or name_offset >= strings_size:
                raise ValueError('Invalid property bounds')
            name_end = data.find(b'\0', strings + name_offset, strings + strings_size)
            if name_end < 0:
                raise ValueError('Unterminated property name')
            name = data[strings + name_offset:name_end].decode('ascii')
            nodes['/' + '/'.join(stack[1:])][name] = data[pos:pos + length]
            pos = (pos + length + 3) & ~3
        elif token == 4:
            continue
        elif token == 9:
            if stack:
                raise ValueError('Unbalanced FDT structure')
            return nodes
        else:
            raise ValueError('Unknown FDT token')
    raise ValueError('FDT end token missing')


def strings(value):
    return [part.decode('ascii') for part in value.rstrip(b'\0').split(b'\0') if part]


def cells(value):
    if len(value) % 4:
        raise ValueError('Unaligned cell property')
    return [u32(value, index) for index in range(0, len(value), 4)]


def reg_ranges(nodes, path):
    parent = path.rsplit('/', 1)[0] or '/'
    props = nodes[parent]
    address_cells = u32(props['#address-cells']) if '#address-cells' in props else 2
    size_cells = u32(props['#size-cells']) if '#size-cells' in props else 1
    if not 1 <= address_cells <= 2 or not 0 <= size_cells <= 2:
        raise ValueError('Unsupported address/size cell count')
    values = cells(nodes[path].get('reg', b''))
    width = address_cells + size_cells
    if len(values) % width:
        raise ValueError('Invalid reg property width: ' + path)
    def join(parts):
        result = 0
        for part in parts:
            result = (result << 32) | part
        return result
    return [{'address': hex(join(values[i:i + address_cells])),
             'size': hex(join(values[i + address_cells:i + width]))}
            for i in range(0, len(values), width)]


def dtbs_from_boot(path):
    raw = path.read_bytes()
    if raw[:8] != b'ANDROID!' or len(raw) < 1660:
        raise ValueError('Invalid legacy Android boot header')
    le = lambda offset: struct.unpack_from('<I', raw, offset)[0]
    if le(40) != 2 or le(36) not in (2048, 4096, 8192, 16384):
        raise ValueError('Expected Android boot header v2')
    page = le(36)
    align = lambda size: (size + page - 1) // page * page
    offset = page + sum(align(le(index)) for index in (8, 16, 24, 1632))
    size = le(1648)
    blob = raw[offset:offset + size]
    if not size or len(blob) != size:
        raise ValueError('Invalid DTB image bounds')
    if u32(blob) == FDT_MAGIC:
        return [blob], {'format': 'raw-fdt', 'image_offset': offset}
    if len(blob) < 32 or u32(blob) != DT_TABLE_MAGIC:
        raise ValueError('Unrecognized DTB container')
    total, header_size, entry_size, count, entries = (u32(blob, i) for i in (4, 8, 12, 16, 20))
    if (total > len(blob) or header_size < 32 or entry_size < 32 or not count or
            entries < header_size or entries + count * entry_size > total):
        raise ValueError('Invalid Android DT table bounds')
    result = []
    for index in range(count):
        entry = entries + index * entry_size
        length, start = u32(blob, entry), u32(blob, entry + 4)
        if length < 40 or start + length > total:
            raise ValueError('Invalid DT table entry')
        result.append(blob[start:start + length])
    return result, {'format': 'android-dt-table', 'image_offset': offset, 'entries': count}


def inventory(nodes):
    root = nodes['/']
    result = {'model': strings(root.get('model', b'')),
              'compatible': strings(root.get('compatible', b'')),
              'cpus': [], 'peripherals': [], 'memory_nodes': [], 'reserved_memory': []}
    for path, props in nodes.items():
        compatible = strings(props.get('compatible', b''))
        if props.get('device_type') == b'cpu\0':
            result['cpus'].append({'path': path, 'compatible': compatible,
                                   'reg': reg_ranges(nodes, path),
                                   'enable_method': strings(props.get('enable-method', b''))})
        if props.get('device_type') == b'memory\0':
            result['memory_nodes'].append({'path': path, 'reg': reg_ranges(nodes, path)})
        if path.startswith('/reserved-memory/'):
            item = {'path': path, 'reg': reg_ranges(nodes, path),
                    'no_map': 'no-map' in props, 'reusable': 'reusable' in props}
            for key in ('size', 'alignment', 'alloc-ranges'):
                if key in props:
                    item[key] = [hex(value) for value in cells(props[key])]
            result['reserved_memory'].append(item)
        selected = ('arm,gic-v3', 'arm,armv8-timer', 'mediatek,mt6779-wdt',
                    'mediatek,mt6779-uart', 'mediatek,mtkfb', 'mediatek,ufshci')
        if any(value in compatible for value in selected):
            item = {'path': path, 'compatible': compatible, 'reg': reg_ranges(nodes, path)}
            if 'interrupts' in props:
                item['interrupt_cells'] = cells(props['interrupts'])
            result['peripherals'].append(item)
    # Boot arguments, serial identifiers, calibration and arbitrary chosen data
    # are intentionally excluded from the public inventory.
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.image.resolve() == args.output.resolve():
        parser.error('Output must not overwrite the input')
    dtbs, container = dtbs_from_boot(args.image)
    report = {'schema_version': 1, 'source': 'stock-boot-dtb-before-bootloader-fixups',
              'source_image_sha256': hashlib.sha256(args.image.read_bytes()).hexdigest(),
              'container': container,
              'inventories': [inventory(parse_fdt(dtb)) for dtb in dtbs],
              'runtime_memory_map_verified': False, 'runtime_framebuffer_verified': False,
              'phone_operations_performed': False}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Wrote filtered inventory for', len(dtbs), 'stock device tree(s).')


if __name__ == '__main__':
    main()
