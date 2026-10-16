#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Inspect an existing legacy Android boot/recovery image without a device."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

KEYS = (
    'CONFIG_ARM64', 'CONFIG_ARCH_MEDIATEK', 'CONFIG_MTK_PLATFORM',
    'CONFIG_IKCONFIG', 'CONFIG_IKCONFIG_PROC', 'CONFIG_KEXEC',
    'CONFIG_KEXEC_FILE', 'CONFIG_KEXEC_CORE', 'CONFIG_ARM_GIC',
    'CONFIG_ARM_GIC_V3', 'CONFIG_ARM_PSCI_FW', 'CONFIG_OF', 'CONFIG_FB',
    'CONFIG_FRAMEBUFFER_CONSOLE', 'CONFIG_EFI',
)


def inspect(path):
    path = Path(path)
    with path.open('rb') as source:
        header = source.read(2048)
        if len(header) < 48 or header[:8] != b'ANDROID!':
            raise ValueError('Expected a legacy Android boot image')
        version = struct.unpack_from('<I', header, 40)[0]
        page = struct.unpack_from('<I', header, 36)[0]
        size = struct.unpack_from('<I', header, 8)[0]
        if version not in (0, 1, 2) or page not in (2048, 4096, 8192, 16384):
            raise ValueError('Unsupported image header version/page size')
        if not 0 < size <= 64 * 1024**2 or page + size > path.stat().st_size:
            raise ValueError('Kernel bounds are invalid')
        source.seek(page)
        kernel = source.read(size)
    compressed = kernel[:2] == b'\x1f\x8b'
    if compressed:
        decoder = zlib.decompressobj(31)
        kernel = decoder.decompress(kernel, 128 * 1024**2)
        if not decoder.eof:
            raise ValueError('Truncated or excessive kernel decompression')
    start = kernel.find(b'IKCFG_ST')
    end = kernel.find(b'IKCFG_ED', start + 8) if start >= 0 else -1
    if end <= start or start < 0:
        raise ValueError('No embedded IKCONFIG found')
    decoder = zlib.decompressobj(31)
    config = decoder.decompress(kernel[start + 8:end], 2 * 1024**2)
    if not decoder.eof:
        raise ValueError('Truncated or excessive configuration decompression')
    found = {}
    lines = config.decode('utf-8').splitlines()
    for key in KEYS:
        for line in lines:
            if line.startswith(key + '='):
                found[key] = line.split('=', 1)[1]
                break
            if line == '# ' + key + ' is not set':
                found[key] = 'n'
                break
    digest = hashlib.sha256()
    with path.open('rb') as source:
        while chunk := source.read(8 * 1024**2):
            digest.update(chunk)
    return {'schema_version': 1, 'image_sha256': digest.hexdigest(),
            'image_size': path.stat().st_size, 'header_version': version,
            'page_size': page, 'kernel_size': size, 'kernel_gzip': compressed,
            'configuration': found, 'phone_operations_performed': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.image.resolve() == args.output.resolve():
        parser.error('Output must not overwrite the input image')
    report = inspect(args.image)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
