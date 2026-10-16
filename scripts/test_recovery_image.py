# SPDX-License-Identifier: BSD-2-Clause
import struct
import unittest

from recovery_image import AndroidV2, Entry, inflate_ramdisk, pack_newc, parse_newc


def entry(name, payload, mode=0o100644, inode=11, links=1):
    return Entry(name, (inode, mode, 1000, 2000, links, 123456, len(payload),
                        3, 4, 5, 6, len(name.encode()) + 1, 0), payload)


class ArchiveTests(unittest.TestCase):
    def test_preserves_symlinks_owners_modes_and_hardlink_identity(self):
        entries = [entry('init', b'system/bin/init', 0o120750),
                   entry('system/bin/a', b'content', inode=12, links=2),
                   entry('system/bin/b', b'', inode=12, links=2)]
        self.assertEqual(parse_newc(pack_newc(entries)), entries)

    def test_rejects_traversal_duplicate_and_unterminated_archive(self):
        for entries in ([entry('../outside', b'bad')],
                        [entry('same', b'a'), entry('same', b'b')]):
            with self.assertRaises(ValueError):
                parse_newc(pack_newc(entries))
        with self.assertRaises(ValueError):
            parse_newc(pack_newc([entry('file', b'body')])[:128])

    def test_rejects_data_size_past_archive_and_nonzero_tail(self):
        raw = bytearray(pack_newc([entry('file', b'body')]))
        raw[54:62] = b'ffffffff'
        with self.assertRaises(ValueError):
            parse_newc(raw)
        raw = bytearray(pack_newc([]))
        raw[-1] = 1
        with self.assertRaises(ValueError):
            parse_newc(raw)

    def test_ramdisk_inflation_has_bound_and_no_concatenated_payload(self):
        import gzip
        packed = gzip.compress(b'hello')
        self.assertEqual(inflate_ramdisk(packed), b'hello')
        for data, limit in ((packed, 4), (packed[:-2], 100), (packed + packed, 100)):
            with self.assertRaises(ValueError):
                inflate_ramdisk(data, limit)


def sample_image():
    page = 2048
    header = bytearray(page)
    header[:8] = b'ANDROID!'
    components = [b'kernel-data', b'old-ramdisk', b'', b'DTBO-original', b'DTB-original']
    for offset, data in zip((8, 16, 24, 1632, 1648), components):
        struct.pack_into('<I', header, offset, len(data))
    struct.pack_into('<II', header, 36, page, 2)
    struct.pack_into('<I', header, 1644, 1660)
    struct.pack_into('<Q', header, 1636, 3 * page)
    result = bytes(header)
    for data in components:
        result += data + bytes((-len(data)) % page)
    return result


class ImageTests(unittest.TestCase):
    def test_growing_ramdisk_moves_dtbo_and_preserves_other_components(self):
        original = AndroidV2(sample_image())
        replacement = b'x' * 5000
        raw = original.replace_ramdisk(replacement)
        changed = AndroidV2(raw)
        self.assertEqual(changed.components['ramdisk'], replacement)
        for name in ('kernel', 'second', 'recovery_dtbo', 'dtb'):
            self.assertEqual(changed.components[name], original.components[name])
        dtbo_offset = struct.unpack_from('<Q', raw, 1636)[0]
        self.assertEqual(dtbo_offset, 5 * 2048)
        self.assertEqual(raw[dtbo_offset:dtbo_offset + 13], b'DTBO-original')
        self.assertEqual(len(raw), 7 * 2048)

    def test_rejects_wrong_dtbo_offset_truncation_and_header_version(self):
        for offset, value in ((1636, 2048), (40, 3)):
            raw = bytearray(sample_image())
            struct.pack_into('<I', raw, offset, value)
            with self.assertRaises(ValueError):
                AndroidV2(raw)
        with self.assertRaises(ValueError):
            AndroidV2(sample_image()[:4000])


if __name__ == '__main__':
    unittest.main()
