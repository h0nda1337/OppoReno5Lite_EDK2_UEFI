#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Package the reviewed DXE/Shell laboratory FD privately; no phone operation.

Separate from the short SEC probes. The BDS timer is an unverified reset request,
not a guaranteed escape from an early exception or an interrupt failure.
"""
import argparse
import json
from pathlib import Path

from inspect_native_firmware import require_native_lab
from prepare_native_probe import digest, repack, source_hashes, validate_transport
from prepare_recovery_diagnostic import PARTITION_SIZE, STOCK_SHA256
from recovery_avb import add_footer
from recovery_image import AndroidV2


def require_lab_manifest(manifest, explicit_lab, direct_entry):
    if (not explicit_lab or not direct_entry or
            manifest.get('purpose') != 'native-dxe-shell-lab' or
            manifest.get('probe_only') is not False or
            manifest.get('reset_request_after_seconds') != 90 or
            manifest.get('kernel_transport') != 'gzip' or
            manifest.get('bootshim_used') is not False):
        raise ValueError('Expected an explicit reviewed DXE/Shell laboratory image')


def validate_candidate(stock, candidate, manifest):
    require_lab_manifest(manifest, True, True)
    validate_transport(stock, candidate, manifest, require_native_lab)


def prepare(stock_path, fd_path, output, avbtool):
    repo = Path(__file__).resolve().parents[1]
    if output.exists() or output.resolve().is_relative_to(repo):
        raise ValueError('Choose a fresh private directory outside the public repository')
    stock, fd = stock_path.read_bytes(), fd_path.read_bytes()
    if len(stock) != PARTITION_SIZE or digest(stock) != STOCK_SHA256:
        raise ValueError('Original recovery backup mismatch')
    inspected = require_native_lab(fd)
    output.mkdir(parents=True)
    image = output/'recovery.img'
    image.write_bytes(repack(stock, fd))
    retained = add_footer(image, stock, avbtool, PARTITION_SIZE)
    candidate = image.read_bytes()
    manifest = {
        'schema_version': 1, 'purpose': 'native-dxe-shell-lab', 'probe_only': False,
        'stock_sha256': STOCK_SHA256, 'candidate_sha256': digest(candidate),
        'native_fd_sha256': inspected['fd_sha256'], 'kernel_transport': 'gzip',
        'compressed_kernel_sha256': digest(AndroidV2(candidate).components['kernel']),
        'reset_request_after_seconds': 90, 'reset_return_path_verified': False,
        'reset_timer_execution_verified_on_phone': False,
        'bootshim_used': False, 'linux_kernel_retained': False,
        'oem_ramdisk_retained': False, 'ramdisk_paths': [],
        'avb_algorithm': 'NONE', 'oem_signature_valid': False,
        'stock_avb_properties_preserved': retained,
        'source_sha256': source_hashes(repo),
        'phone_operations_performed': False, 'physical_execution_verified': False,
    }
    validate_candidate(stock, candidate, manifest)
    (output/'build.json').write_text(json.dumps(manifest, indent=2)+'\n',
                                     encoding='utf-8', newline='\n')
    print('DXE/Shell lab packaged privately and checked; no phone operation performed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-recovery', type=Path, required=True)
    parser.add_argument('--native-lab-fd', type=Path, required=True)
    parser.add_argument('--private-output-dir', type=Path, required=True)
    parser.add_argument('--avbtool', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.stock_recovery, args.native_lab_fd, args.private_output_dir, args.avbtool)


if __name__ == '__main__':
    main()
