#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Wrap the short original SEC probe in a PRIVATE Android v2 recovery.

The native FD replaces the stock Linux kernel using LK's gzip transport. LK
decompresses it before the direct ARM64 entry; no extra loader is appended.
The ramdisk is an empty archive. This tool does not operate on a phone.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import struct
import zlib

from inspect_native_firmware import FD_SIZE, require_entry_probe
from prepare_recovery_diagnostic import PARTITION_SIZE, STOCK_SHA256
from recovery_avb import add_footer, properties
from recovery_image import AndroidV2, align, pack_newc

SOURCE_FILES = (
    'OppoReno5LitePkg/Sec/NativeMain.c',
    'OppoReno5LitePkg/Sec/AArch64/Entry.S',
    'OppoReno5LitePkg/Sec/OppoSec.inf',
    'OppoReno5LitePkg/OppoNative.dsc',
    'OppoReno5LitePkg/OppoNative.fdf',
    'OppoReno5LitePkg/OppoReno5LitePkg.dec',
    'scripts/build_native_firmware.sh',
    'scripts/inspect_native_firmware.py',
    'scripts/prepare_native_probe.py',
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def source_hashes(repo):
    paths = set(SOURCE_FILES)
    paths.update(p.relative_to(repo).as_posix() for p in (repo/'OppoReno5LitePkg').rglob('*') if p.is_file())
    return {p: digest((repo/p).read_bytes()) for p in sorted(paths)}


def probe_ramdisk():
    return gzip.compress(pack_newc([]), compresslevel=9, mtime=0)


def inflate_native_kernel(kernel):
    """Accept one complete gzip member containing exactly the bounded native FD."""
    if not kernel.startswith(b'\x1f\x8b\x08') or len(kernel) > FD_SIZE + 65536:
        raise ValueError('Expected the bounded gzip native kernel transport')
    decoder = zlib.decompressobj(16 + zlib.MAX_WBITS)
    try:
        fd = decoder.decompress(kernel, FD_SIZE + 1)
    except zlib.error as error:
        raise ValueError('Invalid native kernel gzip stream') from error
    if (len(fd) != FD_SIZE or not decoder.eof or decoder.unused_data or
            decoder.unconsumed_tail):
        raise ValueError('Native gzip must contain exactly one complete 8 MiB FD')
    return fd


def repack(stock, fd):
    """Preserve DTB/DTBO/second and all header fields except sizes, ID and offset."""
    original = AndroidV2(stock)
    parts = dict(original.components,
                 kernel=gzip.compress(fd, compresslevel=9, mtime=0),
                 ramdisk=probe_ramdisk())
    header = bytearray(original.header)
    struct.pack_into('<I', header, 8, len(parts['kernel']))
    struct.pack_into('<I', header, 16, len(parts['ramdisk']))
    output = bytearray(header)
    checksum = hashlib.sha1()
    for name in ('kernel', 'ramdisk', 'second', 'recovery_dtbo', 'dtb'):
        data = parts[name]
        if name == 'recovery_dtbo':
            struct.pack_into('<Q', output, 1636, len(output) if data else 0)
        output.extend(data)
        output.extend(bytes(align(len(output), original.page) - len(output)))
        checksum.update(data)
        checksum.update(struct.pack('<I', len(data)))
    output[576:608] = checksum.digest() + bytes(12)
    return bytes(output)


def validate_candidate(stock, candidate, manifest):
    if (manifest.get('purpose') != 'native-sec-entry-probe' or
            manifest.get('probe_only') is not True or
            manifest.get('reset_request_after_seconds') != 10 or
            manifest.get('kernel_transport') != 'gzip' or
            manifest.get('bootshim_used') is not False):
        raise ValueError('Expected a reviewed short SEC probe manifest')
    if digest(stock) != STOCK_SHA256 or len(stock) != PARTITION_SIZE:
        raise ValueError('Original recovery backup mismatch')
    if len(candidate) != PARTITION_SIZE or digest(candidate) != manifest['candidate_sha256']:
        raise ValueError('Native candidate size or digest mismatch')
    changed = AndroidV2(candidate)
    if digest(changed.components['kernel']) != manifest.get('compressed_kernel_sha256'):
        raise ValueError('Compressed native kernel differs from the reviewed probe')
    fd = inflate_native_kernel(changed.components['kernel'])
    require_entry_probe(fd)
    if digest(fd) != manifest['native_fd_sha256']:
        raise ValueError('Native FD differs from the reviewed probe')
    packed = repack(stock, fd)
    if candidate[:len(packed)] != packed or properties(candidate) != properties(stock):
        raise ValueError('Unexpected recovery content or AVB property change')
    repo = Path(__file__).resolve().parents[1]
    expected_sources = source_hashes(repo)
    if manifest.get('source_sha256') != expected_sources:
        raise ValueError('Native probe sources changed after preparation')


def prepare(stock_path, fd_path, output, avbtool):
    repo = Path(__file__).resolve().parents[1]
    if output.exists() or output.resolve().is_relative_to(repo):
        raise ValueError('Choose a fresh private directory outside the public repository')
    stock, fd = stock_path.read_bytes(), fd_path.read_bytes()
    if len(stock) != PARTITION_SIZE or digest(stock) != STOCK_SHA256:
        raise ValueError('Original recovery backup mismatch')
    inspected = require_entry_probe(fd)
    output.mkdir(parents=True)
    image = output/'recovery.img'
    image.write_bytes(repack(stock, fd))
    retained = add_footer(image, stock, avbtool, PARTITION_SIZE)
    manifest = {
        'schema_version': 1, 'purpose': 'native-sec-entry-probe',
        'stock_sha256': STOCK_SHA256, 'candidate_sha256': digest(image.read_bytes()),
        'native_fd_sha256': inspected['fd_sha256'], 'probe_only': True,
        'kernel_transport': 'gzip',
        'compressed_kernel_sha256': digest(AndroidV2(image.read_bytes()).components['kernel']),
        'reset_request_after_seconds': 10, 'reset_return_path_verified': False,
        'bootshim_used': False, 'linux_kernel_retained': False,
        'oem_ramdisk_retained': False, 'ramdisk_paths': [],
        'avb_algorithm': 'NONE', 'oem_signature_valid': False,
        'stock_avb_properties_preserved': retained,
        'source_sha256': source_hashes(repo),
        'phone_operations_performed': False, 'physical_execution_verified': False,
    }
    validate_candidate(stock, image.read_bytes(), manifest)
    (output/'build.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8', newline='\n')
    print('Short native SEC probe packaged privately and checked; no phone operation performed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-recovery', type=Path, required=True)
    parser.add_argument('--native-probe-fd', type=Path, required=True)
    parser.add_argument('--private-output-dir', type=Path, required=True)
    parser.add_argument('--avbtool', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.stock_recovery, args.native_probe_fd, args.private_output_dir, args.avbtool)


if __name__ == '__main__':
    main()
