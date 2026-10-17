# SPDX-License-Identifier: BSD-2-Clause
"""Synthetic container corruption checks; fixtures are not executable firmware."""
import struct
import unittest
from uuid import UUID

from inspect_native_firmware import FD_BASE, FD_SIZE, FV_START, REQUIRED, inspect


def pe_fixture():
    data = bytearray(128)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 64)
    data[64:68] = b'PE\0\0'
    struct.pack_into('<H', data, 68, 0xaa64)
    return data


def file_fixture(identifier, kind, section_type, payload):
    section = (len(payload) + 4).to_bytes(3, 'little') + bytes([section_type]) + payload
    header = bytearray(24)
    header[:16] = UUID(identifier).bytes_le
    header[17], header[18], header[23] = 0xaa, kind, 0xf8
    header[20:23] = (len(section) + 24).to_bytes(3, 'little')
    header[16] = (-(sum(header) - header[17] - header[23])) & 0xff
    return header + section


def fixture():
    raw = bytearray(b'\xff' * FD_SIZE)
    raw[:64] = b'\0' * 64
    struct.pack_into('<IIQQ', raw, 0, 0x14000400, 0xd503201f, 0x80000, FD_SIZE)
    raw[56:60] = b'ARM\x64'
    header = bytearray(72)
    header[16:32] = UUID('8c8ce578-8a3d-4f1c-9935-896185c32dd3').bytes_le
    struct.pack_into('<Q', header, 32, FD_SIZE - FV_START)
    header[40:44] = b'_FVH'
    struct.pack_into('<H', header, 48, len(header))
    header[55] = 2
    struct.pack_into('<II', header, 56, (FD_SIZE - FV_START) // 4096, 4096)
    position = FV_START + len(header)
    te_at = position + 24 + 4
    te = bytearray(44)
    struct.pack_into('<HHBBHIIQ', te, 0, 0x5a56, 0xaa64, 0, 10, 40, 40, 40, FD_BASE + te_at)
    struct.pack_into('<I', te, 40, 0xd503201f)
    sec_entry = te_at + 40
    struct.pack_into('<I', header, 0, 0x14000000 | ((sec_entry - FV_START) // 4))
    checksum = (-sum(struct.unpack('<36H', header))) & 0xffff
    struct.pack_into('<H', header, 50, checksum)
    raw[FV_START:FV_START + len(header)] = header
    for identifier, (_, kind) in REQUIRED.items():
        data = file_fixture(identifier, kind, 0x12 if kind == 3 else 0x10,
                            te if kind == 3 else pe_fixture())
        raw[position:position + len(data)] = data
        position = (position + len(data) + 7) & ~7
    data = file_fixture('d6a2cb7f-6a18-4e2f-b43b-9920a733700a', 5, 0x10, pe_fixture())
    raw[position:position + len(data)] = data
    return raw


class NativeFirmwareTests(unittest.TestCase):
    def test_direct_entry_and_required_roles(self):
        result = inspect(fixture())
        self.assertTrue(result['direct_header_fv_sec_branch_verified'])
        self.assertEqual(len(result['files']), 6)
        self.assertFalse(result['uefi_execution_observed'])
        self.assertFalse(result['bootshim_used'])

    def test_corrupt_header_checksum_file_bounds_and_sec_entry_are_rejected(self):
        original = fixture()
        reference = inspect(original)
        sec = next(item for item in reference['files'] if item.get('role') == 'original-sec')
        te = sec['sections'][0]['offset']
        for offset, data in ((0, b'\0' * 4), (FV_START + 56, b'\0'),
                             (sec['offset'] + 20, b'\xfe\xff\xff'),
                             (te + 8, struct.pack('<I', 0xffffffff))):
            with self.subTest(offset=offset):
                raw = original.copy()
                raw[offset:offset + len(data)] = data
                with self.assertRaises(ValueError):
                    inspect(raw)

    def test_truncated_container_rejected(self):
        with self.assertRaises(ValueError):
            inspect(fixture()[:-1])


if __name__ == '__main__':
    unittest.main()
