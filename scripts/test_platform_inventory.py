#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Bounds and public-data filtering checks using a synthetic device tree."""
import struct
import unittest

from inspect_platform import inventory, parse_fdt


def fixture():
    table = bytearray()
    block = bytearray()
    offsets = {}
    def word(value):
        block.extend(struct.pack('>I', value))
    def node(name):
        word(1)
        block.extend(name.encode() + b'\0')
        block.extend(b'\0' * (-len(block) % 4))
    def prop(name, value):
        if name not in offsets:
            offsets[name] = len(table)
            table.extend(name.encode() + b'\0')
        word(3)
        word(len(value))
        word(offsets[name])
        block.extend(value)
        block.extend(b'\0' * (-len(block) % 4))
    node('')
    prop('model', b'Example board\0')
    prop('compatible', b'example,board\0')
    prop('#address-cells', struct.pack('>I', 2))
    prop('#size-cells', struct.pack('>I', 2))
    prop('serial-number', b'EXAMPLE_PRIVATE_VALUE\0')
    node('chosen')
    prop('bootargs', b'EXAMPLE_PRIVATE_ARGUMENTS\0')
    word(2)
    node('memory@40000000')
    prop('device_type', b'memory\0')
    prop('reg', struct.pack('>4I', 0, 0x40000000, 0, 0x10000000))
    word(2)
    word(2)
    word(9)
    structs = 56
    strings = structs + len(block)
    header = struct.pack('>10I', 0xD00DFEED, strings + len(table), structs,
                         strings, 40, 17, 16, 0, len(table), len(block))
    return header + b'\0' * 16 + block + table


class InventoryTests(unittest.TestCase):
    def test_memory_cells_and_private_fields(self):
        report = inventory(parse_fdt(fixture()))
        self.assertEqual(report['memory_nodes'][0]['reg'],
                         [{'address': '0x40000000', 'size': '0x10000000'}])
        self.assertNotIn('EXAMPLE_PRIVATE', repr(report))

    def test_truncated_blob_rejected(self):
        with self.assertRaises(ValueError):
            parse_fdt(fixture()[:-4])

    def test_invalid_structure_bounds_rejected(self):
        blob = bytearray(fixture())
        struct.pack_into('>I', blob, 8, len(blob) + 1)
        with self.assertRaises(ValueError):
            parse_fdt(blob)

    def test_bad_reg_width_rejected(self):
        nodes = parse_fdt(fixture())
        nodes['/memory@40000000']['reg'] = b'\0' * 4
        with self.assertRaises(ValueError):
            inventory(nodes)


if __name__ == '__main__':
    unittest.main()
