#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Independent byte-layout checks; consumes only synthetic host test output."""
from pathlib import Path
import struct


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def u64(data, offset):
    return struct.unpack_from('<Q', data, offset)[0]


def verify(path, physical, order, addresses):
    data = path.read_bytes()
    assert data[:8] == b'RSD PTR ' and data[15] == 2
    assert sum(data[:20]) % 256 == sum(data[:36]) % 256 == 0
    assert u32(data, 16) == 0 and u32(data, 20) == 36 and data[33:36] == bytes(3)
    xsdt = u64(data, 24) - physical

    def table(offset, signature, revision):
        assert offset % 8 == 0 and data[offset:offset+4] == signature
        length = u32(data, offset+4)
        assert 36 <= length <= len(data)-offset and data[offset+8] == revision
        result = data[offset:offset+length]
        assert sum(result) % 256 == 0
        assert result[10:16] == b'OPNATV' and result[16:24] == b'CPH2205 '
        return result

    x = table(xsdt, b'XSDT', 1)
    assert len(x) == 60
    fadt, madt, gtdt = [u64(x, p)-physical for p in (36, 44, 52)]
    f = table(fadt, b'FACP', 6)
    assert len(f) == 276 and u32(f, 36) == u64(f, 132) == 0  # no FACS
    assert f[45] == 8 and u32(f, 112) == 1 << 20  # only hardware reduced
    assert f[129:132] == b'\x01\0\0'  # PSCI SMC, minor0
    assert f[46:112] == bytes(66) and f[116:129] == bytes(13)
    assert f[148:] == bytes(len(f)-148)  # no invented PM/RTC/reset registers
    dsdt = u64(f, 140)-physical
    assert u32(f, 40) == (physical+dsdt if physical+dsdt <= 0xffffffff else 0)
    d = table(dsdt, b'DSDT', 2)
    assert len(d) == 332
    # A real AML parser/disassembler is also run by test_acpi.sh.
    a = table(madt, b'APIC', 4)
    assert len(a) == 708 and a[36:44] == bytes(8)
    for uid, (mpidr, redist) in enumerate(zip(order, addresses)):
        c = a[44+uid*80:44+(uid+1)*80]
        assert c[:4] == b'\x0b\x50\0\0'
        assert u32(c, 4) == 0 and u32(c, 8) == uid and u32(c, 12) == 1
        assert u32(c, 16) == 0 and u32(c, 20) == 23
        assert c[24:56] == bytes(32) and u32(c, 56) == 25
        assert u64(c, 60) == redist and u64(c, 68) == mpidr and c[76:] == bytes(4)
    dist = a[-24:]
    assert dist[:8] == b'\x0c\x18'+bytes(6)
    assert u64(dist, 8) == 0x0c000000 and u32(dist, 16) == 0 and dist[20:] == b'\x03\0\0\0'
    g = table(gtdt, b'GTDT', 2)
    assert len(g) == 96 and u64(g, 36) == u64(g, 80) == 0xffffffffffffffff
    assert u32(g, 44) == 0 and g[88:] == bytes(8)
    for pos, irq in zip((48, 56, 64, 72), (29, 30, 27, 26)):
        assert u32(g, pos) == irq and u32(g, pos+4) == 2
    ranges = [(0, 36), (xsdt, len(x)), (fadt, len(f)), (dsdt, len(d)), (madt, len(a)), (gtdt, len(g))]
    used = set()
    for start, length in ranges:
        cells = set(range(start, start+length))
        assert not cells & used
        used |= cells
    assert all(byte == 0 for i, byte in enumerate(data) if i not in used)


def main():
    root = Path('.build/acpi')
    order = [i << 8 for i in range(8)]
    addresses = [0x0c040000+i*0x20000 for i in range(8)]
    verify(root/'low.bin', 0x90000000, order, addresses)
    verify(root/'high.bin', 0x100000000, order, addresses)
    last = addresses.copy(); last[7] = 0x0c220000
    verify(root/'last-frame.bin', 0x90000000, order, last)
    order[0], order[6] = order[6], order[0]
    addresses[0], addresses[6] = addresses[6], addresses[0]
    addresses[2], addresses[5] = addresses[5], addresses[2]
    verify(root/'shuffled.bin', 0x90000000, order, addresses)
    print('Independent ACPI byte checks: all pointers, checksums, layouts, IRQs, reserved bytes and shuffled CPU mapping passed')


if __name__ == '__main__':
    main()
