# SPDX-License-Identifier: BSD-2-Clause
import struct
import unittest

from inspect_runtime import framebuffer_tag, framebuffer_tags_agree, memory_range, reservations


class RuntimeTests(unittest.TestCase):
    def test_native_little_endian_framebuffer_above_4g(self):
        chosen = {'atag,videolfb': struct.pack('<QIII', 0x13e000000, 1, 6000, 0x2000000),
                  'bootargs': b'private', 'serial-number': b'private'}
        result = framebuffer_tag(chosen)
        self.assertEqual(result[0]['address'], '0x13e000000')
        self.assertEqual(result[0]['size'], '0x2000000')
        self.assertNotIn('private', str(result))

    def test_big_endian_split_properties_and_conflicts(self):
        chosen = {'atag,videolfb-' + name: struct.pack('>I', value)
                  for name, value in [('fb_base_h', 1), ('fb_base_l', 0x3e000000),
                                      ('vramSize', 0x2000000)]}
        self.assertEqual(framebuffer_tag(chosen)[0]['address'], '0x13e000000')
        chosen['atag,videolfb'] = struct.pack('<QIII', 0x80000000, 1, 6000, 0x2000000)
        with self.assertRaises(ValueError):
            framebuffer_tag(chosen)
        observed = framebuffer_tag(chosen, allow_conflicts=True)
        self.assertEqual(len(observed), 2)
        self.assertFalse(framebuffer_tags_agree(observed))
        self.assertTrue(framebuffer_tags_agree(observed[:1]))
        with self.assertRaises(ValueError):
            framebuffer_tag({'atag,videolfb': b'short'})

    def test_ranges_reject_wraparound_zero_and_negative(self):
        for base, size in ((0, 0), (2**64 - 1, 2), (-1, 4096), (0, -1)):
            with self.assertRaises(ValueError):
                memory_range(base, size)

    def test_memreserve_is_big_endian_and_must_terminate(self):
        raw = bytearray(128)
        struct.pack_into('>IIIIIIIIII', raw, 0, 0xd00dfeed, 128, 88, 112, 40,
                         17, 16, 0, 16, 24)
        struct.pack_into('>QQ', raw, 40, 0x100000000, 0x200000)
        self.assertEqual(reservations(raw), [{'address': '0x100000000', 'size': '0x200000'}])
        struct.pack_into('>QQ', raw, 56, 0x50000000, 0x1000)
        struct.pack_into('>QQ', raw, 72, 0x60000000, 0x1000)
        with self.assertRaises(ValueError):
            reservations(raw)


if __name__ == '__main__':
    unittest.main()
