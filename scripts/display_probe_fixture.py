#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Generate a public synthetic MT6779 binding fixture, without device data."""
from pathlib import Path
import struct
import sys

def fixture():
    table, block, offsets = bytearray(), bytearray(), {}
    def word(value): block.extend(struct.pack('>I', value))
    def node(name):
        word(1)
        block.extend(name.encode() + b'\0')
        block.extend(b'\0' * (-len(block) % 4))
    def prop(name, value):
        if name not in offsets:
            offsets[name] = len(table)
            table.extend(name.encode() + b'\0')
        word(3); word(len(value)); word(offsets[name])
        block.extend(value)
        block.extend(b'\0' * (-len(block) % 4))
    node('')
    prop('compatible', b'mediatek,MT6779\0')
    node('soc')
    prop('#address-cells', struct.pack('>I', 2))
    prop('#size-cells', struct.pack('>I', 2))
    prop('ranges', b'')
    for name, compatible, base in [
        ('clock-controller@14000000', b'mediatek,mt6779-mmsys\0syscon\0', 0x14000000),
        ('disp_ovl0@14008000', b'mediatek,mt6779-disp_ovl0\0', 0x14008000),
        ('disp_ovl0_2l@14009000', b'mediatek,mt6779-disp_ovl0_2l\0', 0x14009000),
        ('disp_ovl1_2l@1400a000', b'mediatek,mt6779-disp_ovl1_2l\0', 0x1400a000)]:
        node(name)
        prop('compatible', compatible)
        prop('reg', struct.pack('>QQ', base, 0x1000))
        word(2)
    word(2); word(2); word(9)
    structs = 56
    strings = structs + len(block)
    return struct.pack('>10I', 0xd00dfeed, strings+len(table), structs, strings,
                       40, 17, 16, 0, len(table), len(block)) + bytes(16) + block + table

if __name__ == '__main__':
    Path(sys.argv[1]).write_bytes(fixture())
