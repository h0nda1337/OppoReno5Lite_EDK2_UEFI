# SPDX-License-Identifier: BSD-2-Clause
import struct
import unittest
import zlib

from extract_stock_logo import bitmap24, crop_band, decode_frame


class LogoTests(unittest.TestCase):
    def test_bmp_bottom_up_bgr_order_and_four_byte_row_padding(self):
        pixels = bytes([1, 2, 3, 255, 4, 5, 6, 255])
        bmp = bitmap24(pixels, 1, 2)
        self.assertEqual(bmp[54:], bytes([4, 5, 6, 0, 1, 2, 3, 0]))
        self.assertEqual(struct.unpack_from('<I', bmp, 2)[0], len(bmp))
        self.assertEqual(struct.unpack_from('<H', bmp, 28)[0], 24)

    def test_content_band_does_not_merge_separate_tagline(self):
        black = bytes([0, 0, 0, 255])
        white = bytes([255] * 4)
        pixels = black * 3 + black + white + black + black * 3 + white * 3
        result, width, height, origin, bands = crop_band(pixels, 3, 4, 0)
        self.assertEqual((result, width, height, origin), (white, 1, 1, (1, 1)))
        self.assertEqual(bands, [(1, 2), (3, 4)])

    def test_logo_container_bounds_and_frame_decompression_limit(self):
        payload = struct.pack('<III', 1, 12 + len(zlib.compress(b'abcd')), 12) + zlib.compress(b'abcd')
        header = bytearray(512)
        struct.pack_into('<II', header, 0, 0x58881688, len(payload))
        header[8:13] = b'logo\0'
        struct.pack_into('<I', header, 52, 512)
        blob = bytes(header) + payload
        self.assertEqual(decode_frame(blob, 0, 1, 1), (b'abcd', 1))
        for data, index, width in ((blob, 1, 1), (blob[:-1], 0, 1), (blob, 0, 2)):
            with self.assertRaises(ValueError):
                decode_frame(data, index, width, 1)


if __name__ == '__main__':
    unittest.main()
