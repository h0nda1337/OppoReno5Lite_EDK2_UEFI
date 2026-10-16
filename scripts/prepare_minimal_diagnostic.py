#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Build a private stock-kernel recovery with an original minimal Linux init.

No proprietary ramdisk program is retained. This is hardware inventory under
Linux, not an EDK2 image. Does not operate on a phone.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import struct

from recovery_avb import add_footer
from prepare_recovery_diagnostic import PARTITION_SIZE, STOCK_SHA256
from recovery_image import AndroidV2, Entry, pack_newc, parse_newc


def minimal_entries(binary):
    if (binary[:6] != b'\x7fELF\x02\x01' or len(binary) > 4 * 1024**2 or
            len(binary) < 64 or struct.unpack_from('<H', binary, 18)[0] != 183):
        raise ValueError('Expected a bounded ELF64 little-endian AArch64 init')
    entries = []
    for index, name in enumerate(('dev', 'proc', 'sys', 'tmp', 'config', 'init'), 1):
        data = binary if name == 'init' else b''
        mode = 0o100755 if name == 'init' else 0o040755
        fields = (index, mode, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(name) + 1, 0)
        entries.append(Entry(name, fields, data))
    return entries


def prepare(stock, binary, output, avbtool):
    repo = Path(__file__).resolve().parents[1]
    if output.exists() or output.resolve().is_relative_to(repo):
        raise ValueError('Choose a fresh private output directory outside the public repository')
    raw = stock.read_bytes()
    if len(raw) != PARTITION_SIZE or hashlib.sha256(raw).hexdigest() != STOCK_SHA256:
        raise ValueError('Original recovery backup mismatch')
    init = binary.read_bytes()
    entries = minimal_entries(init)
    archive = pack_newc(entries)
    if parse_newc(archive) != entries:
        raise ValueError('Minimal initramfs round trip failed')
    original = AndroidV2(raw)
    packed = original.replace_ramdisk(gzip.compress(archive, compresslevel=9, mtime=0))
    changed = AndroidV2(packed)
    if any(original.components[name] != changed.components[name]
           for name in ('kernel', 'second', 'recovery_dtbo', 'dtb')):
        raise ValueError('Unexpected non-ramdisk modification')
    output.mkdir(parents=True)
    image = output / 'recovery.img'
    image.write_bytes(packed)
    retained = add_footer(image, raw, avbtool, PARTITION_SIZE)
    report = {'schema_version': 1, 'purpose': 'minimal-linux-readout',
              'stock_sha256': STOCK_SHA256,
              'candidate_sha256': hashlib.sha256(image.read_bytes()).hexdigest(),
              'init_sha256': hashlib.sha256(init).hexdigest(),
              'init_source_sha256': hashlib.sha256((repo / 'scripts/capture_init.c').read_bytes()).hexdigest(),
              'ramdisk_paths': [entry.name for entry in entries],
              'unchanged_components': {name: hashlib.sha256(changed.components[name]).hexdigest()
                                       for name in ('kernel', 'second', 'recovery_dtbo', 'dtb')},
              'linux_diagnostic': True, 'uefi_firmware': False,
              'avb_algorithm': 'NONE', 'oem_signature_valid': False,
              'stock_avb_properties_preserved': retained,
              'persistent_block_writes_in_init': False,
              'usb_transport': 'CDC-ACM', 'usb_identity': 'OPPO-UEFI-DIAG',
              'timeout_return_to_android_seconds': 180,
              'phone_operations_performed': False, 'physical_execution_verified': False}
    (output / 'build.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-recovery', type=Path, required=True)
    parser.add_argument('--linux-init', type=Path, required=True)
    parser.add_argument('--private-output-dir', type=Path, required=True)
    parser.add_argument('--avbtool', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.stock_recovery, args.linux_init,
                             args.private_output_dir, args.avbtool), indent=2))


if __name__ == '__main__':
    main()
