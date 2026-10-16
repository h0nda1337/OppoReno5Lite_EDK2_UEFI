# SPDX-License-Identifier: BSD-2-Clause
import struct
import unittest

from recovery_avb import properties


def fixture(pairs):
    descriptors = b''
    for key, value in pairs:
        key, value = key.encode(), value.encode()
        body = struct.pack('>QQ', len(key), len(value)) + key + b'\0' + value + b'\0'
        body += bytes((-len(body)) % 8)
        descriptors += struct.pack('>QQ', 0, len(body)) + body
    header = bytearray(256)
    header[:4] = b'AVB0'
    struct.pack_into('>QQ', header, 12, 0, len(descriptors))
    struct.pack_into('>QQ', header, 96, 0, len(descriptors))
    metadata = header + descriptors
    prefix = bytes(64) + metadata
    return prefix + bytes(4096 - 64 - len(prefix)) + struct.pack('>4sIIQQQ28x', b'AVBf', 1, 0, 64, 64, len(metadata))


class AvbPropertyTests(unittest.TestCase):
    def test_preserves_all_version_and_patch_properties(self):
        source = [('com.android.build.boot.security_patch', '2026-01-01'),
                  ('com.android.build.system.os_version', '12'),
                  ('com.android.build.vendor.security_patch', '2026-01-01')]
        self.assertEqual(properties(fixture(source)), dict(source))

    def test_rejects_footer_and_auxiliary_bounds(self):
        valid = fixture([('example', 'value')])
        outside = bytearray(valid)
        struct.pack_into('>Q', outside, len(outside) - 44, 4096)
        bad_aux = bytearray(valid)
        struct.pack_into('>Q', bad_aux, 64 + 104, 2**63)
        for raw in (b'', valid[:-1], outside, bad_aux):
            with self.assertRaises(ValueError):
                properties(raw)

    def test_rejects_duplicate_property_and_descriptor_overflow(self):
        duplicate = fixture([('example', 'one'), ('example', 'two')])
        overflow = bytearray(fixture([('example', 'one')]))
        struct.pack_into('>Q', overflow, 64 + 256 + 8, 2**63)
        for raw in (duplicate, overflow):
            with self.assertRaises(ValueError):
                properties(raw)

    def test_rejects_bad_terminator_and_nonzero_padding(self):
        valid = fixture([('example', 'one')])
        terminator = bytearray(valid)
        terminator[64 + 256 + 32 + 7] = 1
        padding = bytearray(valid)
        padding[64 + 256 + 47] = 1
        for raw in (terminator, padding):
            with self.assertRaises(ValueError):
                properties(raw)
