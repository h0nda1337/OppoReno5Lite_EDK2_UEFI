#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Decode an owner's MediaTek logo frame into a private BMP for future BGRT use.

Only format conversion and an explicitly selected content band are performed.
Does not generate artwork, access a phone or publish the manufacturer's assets.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


def decode_frame(blob, index, width, height):
    if not 0 < width <= 4096 or not 0 < height <= 4096 or width * height * 4 > 32 * 1024**2:
        raise ValueError('Unsupported frame geometry')
    if len(blob) < 512 or struct.unpack_from('<I', blob)[0] != 0x58881688:
        raise ValueError('Expected MediaTek image header')
    size = struct.unpack_from('<I', blob, 4)[0]
    if blob[8:13] != b'logo\0' or struct.unpack_from('<I', blob, 52)[0] != 512:
        raise ValueError('Unexpected logo image name/header size')
    body = blob[512:512 + size]
    if len(body) != size or len(body) < 8:
        raise ValueError('Truncated logo payload')
    count, total = struct.unpack_from('<II', body)
    if not 0 < count <= 512 or total != size or 8 + count * 4 > size or not 0 <= index < count:
        raise ValueError('Invalid logo table bounds or frame index')
    offsets = list(struct.unpack_from('<' + 'I' * count, body, 8)) + [total]
    if offsets[0] < 8 + count * 4 or any(a >= b for a, b in zip(offsets, offsets[1:])):
        raise ValueError('Invalid logo frame offsets')
    decoder = zlib.decompressobj()
    expected = width * height * 4
    pixels = decoder.decompress(body[offsets[index]:offsets[index + 1]], expected + 1)
    if len(pixels) != expected or not decoder.eof or decoder.unused_data:
        raise ValueError('Logo frame is not the expected bounded BGRA raster')
    return pixels, count


def content_bands(pixels, width, height):
    bands, start = [], None
    for y in range(height):
        row = pixels[y * width * 4:(y + 1) * width * 4]
        present = any(row[0::4]) or any(row[1::4]) or any(row[2::4])
        if present and start is None:
            start = y
        elif not present and start is not None:
            bands.append((start, y))
            start = None
    if start is not None:
        bands.append((start, height))
    return bands


def crop_band(pixels, width, height, band):
    bands = content_bands(pixels, width, height)
    if not 0 <= band < len(bands):
        raise ValueError('Selected content band does not exist')
    top, bottom = bands[band]
    left, right = width, 0
    for y in range(top, bottom):
        for x in range(width):
            offset = (y * width + x) * 4
            if any(pixels[offset:offset + 3]):
                left, right = min(left, x), max(right, x + 1)
    cropped = b''.join(pixels[(y * width + left) * 4:(y * width + right) * 4]
                       for y in range(top, bottom))
    return cropped, right - left, bottom - top, (left, top), bands


def bitmap24(pixels, width, height):
    if len(pixels) != width * height * 4 or width <= 0 or height <= 0:
        raise ValueError('Bitmap raster size mismatch')
    stride = (width * 3 + 3) & ~3
    image_size = stride * height
    header = b'BM' + struct.pack('<IHHI', 54 + image_size, 0, 0, 54)
    header += struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, image_size, 0, 0, 0, 0)
    result = bytearray(header)
    for y in range(height - 1, -1, -1):
        row = pixels[y * width * 4:(y + 1) * width * 4]
        for x in range(width):
            result.extend(row[x * 4:x * 4 + 3])
        result.extend(bytes(stride - width * 3))
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('private_logo_image', type=Path)
    parser.add_argument('--private-output-dir', type=Path, required=True)
    parser.add_argument('--width', type=int, required=True)
    parser.add_argument('--height', type=int, required=True)
    parser.add_argument('--frame', type=int, default=0)
    parser.add_argument('--band', type=int, help='Select a reviewed vertical content band; e.g. wordmark without Android tagline')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    if args.private_output_dir.exists() or args.private_output_dir.resolve().is_relative_to(repo):
        parser.error('Choose a fresh private output directory outside the public repository')
    blob = args.private_logo_image.read_bytes()
    pixels, count = decode_frame(blob, args.frame, args.width, args.height)
    width, height, origin = args.width, args.height, (0, 0)
    bands = content_bands(pixels, width, height)
    if args.band is not None:
        pixels, width, height, origin, bands = crop_band(pixels, width, height, args.band)
    bmp = bitmap24(pixels, width, height)
    report = {'schema_version': 1, 'source_logo_sha256': hashlib.sha256(blob).hexdigest(),
              'container_frames': count, 'frame_index': args.frame,
              'source_resolution': [args.width, args.height],
              'content_bands_y': bands, 'selected_band': args.band,
              'bitmap_resolution': [width, height], 'source_origin_xy': origin,
              'bitmap_sha256': hashlib.sha256(bmp).hexdigest(),
              'bitmap_format': '24-bit-BI_RGB-bottom-up',
              'uefi_rendering_verified': False, 'bgrt_installed': False,
              'windows_boot_verified': False, 'phone_operations_performed': False}
    args.private_output_dir.mkdir(parents=True)
    (args.private_output_dir / 'oppo-logo.bmp').write_bytes(bmp)
    (args.private_output_dir / 'extraction.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
