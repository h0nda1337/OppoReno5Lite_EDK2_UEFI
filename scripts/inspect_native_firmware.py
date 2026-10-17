#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Inspect the original integration FD without executing it or using a phone.

Checks ARM64 handoff metadata, root FV/FFS bounds and direct SEC entry. A valid
container is not evidence that DXE, GOP, PSCI or the Shell works on hardware.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from uuid import UUID

FD_BASE = 0x40080000
FD_SIZE = 0x800000
FV_START = 0x1000
ENTRY_PROBE_MARKER = b'OPPO_SEC_PROBE_ONLY_V1\0'
MMU_PROBE_MARKER = b'OPPO_SEC_MMU_PROBE_ONLY_V1\0'
PROBE_MARKERS = {'handoff': ENTRY_PROBE_MARKER, 'mmu': MMU_PROBE_MARKER}
NATIVE_LAB_MARKER = b'OPPO_NATIVE_DXE_SHELL_LAB_90S_V1\0'
REQUIRED = {
    '2bf0a605-3974-4d50-b19b-e0b1a6a4667e': ('original-sec', 3),
    'a9b2bfc1-6e2f-4f4c-9589-c6aa3f851d67': ('original-gop', 7),
    '799bc15f-c3a6-4d88-94bc-809f07c356ab': ('original-runtime', 7),
    '1d4ea2cd-6d78-44ad-8c78-aa6b3d286273': ('original-bds', 7),
    '7c04a583-9e3e-4f1c-ad65-e05268d0b4d1': ('edk2-shell', 9),
}


def branch_target(raw, position):
    word = struct.unpack_from('<I', raw, position)[0]
    if word & 0xfc000000 != 0x14000000:
        raise ValueError('Expected a direct AArch64 branch')
    immediate = word & 0x3ffffff
    if immediate & 0x2000000:
        immediate -= 0x4000000
    target = position + immediate * 4
    if not 0 <= target < len(raw) or target % 4:
        raise ValueError('Branch target outside image or unaligned')
    return target


def sections(raw, start, end):
    result = []
    while start < end:
        if end - start < 4:
            if any(raw[start:end]):
                raise ValueError('Unexpected section tail')
            break
        size, kind = int.from_bytes(raw[start:start + 3], 'little'), raw[start + 3]
        header = 4
        if size == 0xffffff:
            if end - start < 8:
                raise ValueError('Truncated extended section')
            size, header = struct.unpack_from('<I', raw, start + 4)[0], 8
        if size < header or size > end - start:
            raise ValueError('Section exceeds its FFS file')
        result.append({'type': kind, 'offset': start + header, 'size': size - header})
        start = (start + size + 3) & ~3
    return result


def inspect(raw):
    if len(raw) != FD_SIZE or raw[56:60] != b'ARM\x64':
        raise ValueError('Native FD size or ARM64 header mismatch')
    if struct.unpack_from('<QQ', raw, 8) != (0x80000, FD_SIZE):
        raise ValueError('Unexpected ARM64 text offset/image size')
    if branch_target(raw, 0) != FV_START:
        raise ValueError('Image header does not branch directly into root FV')
    if raw[FV_START + 40:FV_START + 44] != b'_FVH':
        raise ValueError('Missing firmware volume header')
    length = struct.unpack_from('<Q', raw, FV_START + 32)[0]
    header = struct.unpack_from('<H', raw, FV_START + 48)[0]
    if length != len(raw) - FV_START or not 56 <= header <= length or header % 2:
        raise ValueError('Invalid firmware volume/header bounds')
    words = struct.unpack_from('<' + 'H' * (header // 2), raw, FV_START)
    if sum(words) & 0xffff:
        raise ValueError('Firmware volume header checksum failed')
    sec_target = branch_target(raw, FV_START)
    files, position, end = [], (FV_START + header + 7) & ~7, len(raw)
    while position + 24 <= end:
        if raw[position:position + 24] == b'\xff' * 24:
            if raw[position:] != b'\xff' * (end - position):
                raise ValueError('Unexpected data after free firmware space')
            break
        kind, attributes = raw[position + 18:position + 20]
        size, file_header = int.from_bytes(raw[position + 20:position + 23], 'little'), 24
        if size == 0xffffff:
            if end - position < 32 or not attributes & 1:
                raise ValueError('Invalid extended FFS header')
            size, file_header = struct.unpack_from('<Q', raw, position + 24)[0], 32
        if size < file_header or size > end - position:
            raise ValueError('FFS file exceeds the root firmware volume')
        # Header checksum excludes its state and file-checksum bytes.
        if (sum(raw[position:position + file_header]) - raw[position + 17] - raw[position + 23]) & 0xff:
            raise ValueError('FFS header checksum failed')
        if attributes & 0x40 and (sum(raw[position + file_header:position + size]) + raw[position + 17]) & 0xff:
            raise ValueError('FFS data checksum failed')
        if kind != 0xf0:
            identifier = str(UUID(bytes_le=bytes(raw[position:position + 16])))
            entry = {'guid': identifier, 'type': kind, 'offset': position, 'size': size,
                     'sections': sections(raw, position + file_header, position + size)}
            if identifier in REQUIRED:
                name, expected_type = REQUIRED[identifier]
                if kind != expected_type:
                    raise ValueError('Required module has an unexpected FFS type')
                entry['role'] = name
            files.append(entry)
        position = (position + size + 7) & ~7
    seen = [item['guid'] for item in files]
    if len(seen) != len(set(seen)) or not set(REQUIRED).issubset(seen):
        raise ValueError('Duplicate or missing required firmware files')
    if sum(item['type'] == 5 for item in files) != 1:
        raise ValueError('Expected one DXE core')
    sec = next(item for item in files if item.get('role') == 'original-sec')
    te = [item for item in sec['sections'] if item['type'] == 0x12]
    if len(te) != 1 or te[0]['size'] < 40:
        raise ValueError('Expected one SEC TE section')
    te_offset = te[0]['offset']
    if raw[te_offset:te_offset + 2] != b'VZ' or struct.unpack_from('<H', raw, te_offset + 2)[0] != 0xaa64:
        raise ValueError('SEC is not an AArch64 TE image')
    stripped = struct.unpack_from('<H', raw, te_offset + 6)[0]
    entry_rva = struct.unpack_from('<I', raw, te_offset + 8)[0]
    image_base = struct.unpack_from('<Q', raw, te_offset + 16)[0]
    entry_offset = te_offset + 40 - stripped + entry_rva
    if not te_offset + 40 <= entry_offset < te_offset + te[0]['size'] or entry_offset != sec_target:
        raise ValueError('Root FV branch does not enter the SEC image directly')
    if image_base + entry_rva != FD_BASE + entry_offset:
        raise ValueError('SEC image base/entry rebasing mismatch')
    for module in files:
        for section in module['sections']:
            if section['type'] == 0x10:
                data = raw[section['offset']:section['offset'] + section['size']]
                if len(data) < 64 or data[:2] != b'MZ':
                    raise ValueError('Invalid module PE header')
                pe = struct.unpack_from('<I', data, 60)[0]
                if pe > len(data) - 24 or data[pe:pe + 4] != b'PE\0\0' or struct.unpack_from('<H', data, pe + 4)[0] != 0xaa64:
                    raise ValueError('Module PE bounds or architecture mismatch')
    return {'schema_version': 1, 'scope': 'native integration prototype host inspection',
            'fd_size': len(raw), 'fd_sha256': hashlib.sha256(raw).hexdigest(),
            'fd_base': hex(FD_BASE), 'root_fv_offset': FV_START,
            'root_fv_checksum_valid': True, 'direct_header_fv_sec_branch_verified': True,
            'sec_entry_physical_address': hex(FD_BASE + entry_offset),
            'files': files, 'bootshim_used': False,
            'uefi_execution_observed': False, 'phone_operations_performed': False}


def require_entry_probe(raw, phase='handoff'):
    if phase not in PROBE_MARKERS:
        raise ValueError('Unknown native probe phase')
    report = inspect(raw)
    sec = next(item for item in report['files'] if item.get('role') == 'original-sec')
    te = next(item for item in sec['sections'] if item['type'] == 0x12)
    payload = raw[te['offset']:te['offset'] + te['size']]
    if (payload.count(PROBE_MARKERS[phase]) != 1 or
            any(marker in payload for name, marker in PROBE_MARKERS.items() if name != phase)):
        raise ValueError('Expected only the selected short SEC probe, not another mode')
    return report


def require_native_lab(raw):
    report = inspect(raw)
    sec = next(item for item in report['files'] if item.get('role') == 'original-sec')
    te = next(item for item in sec['sections'] if item['type'] == 0x12)
    payload = raw[te['offset']:te['offset'] + te['size']]
    if any(marker in payload for marker in PROBE_MARKERS.values()):
        raise ValueError('Short SEC probes cannot run as the DXE/Shell lab')
    bds = next(item for item in report['files'] if item.get('role') == 'original-bds')
    executables = [item for item in bds['sections'] if item['type'] == 0x10]
    if len(executables) != 1:
        raise ValueError('Expected one BDS executable')
    pe = executables[0]
    if raw[pe['offset']:pe['offset'] + pe['size']].count(NATIVE_LAB_MARKER) != 1:
        raise ValueError('Expected the reviewed 90-second laboratory BDS marker')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('private_fd', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.private_fd.resolve() == args.output.resolve():
        parser.error('Output must not overwrite the firmware')
    report = inspect(args.private_fd.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Native image metadata, modules and direct SEC entry verified on host; no execution proved.')


if __name__ == '__main__':
    main()
