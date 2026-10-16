#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Prepare a PRIVATE recovery diagnostic candidate; never operates on a phone.

Preserves the original kernel, DTB, recovery DTBO and every unrelated ramdisk
entry. Enables diagnostic ADB in this candidate only; restore stock afterwards.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path

from recovery_avb import add_footer
from recovery_image import AndroidV2, Entry, inflate_ramdisk, pack_newc, parse_newc

STOCK_SHA256 = 'ac989e3009148792101a764a82fca8ca6212ec0afdd84e13d3b85e87364903b1'
PARTITION_SIZE = 64 * 1024**2
RC_PATH = 'system/etc/init/hw/init.rc'
SCRIPT_PATH = 'system/etc/oppo-uefi-capture.sh'
RC_APPEND = b'''
# OPPO EDK2 diagnostic: writes observations to tmpfs only.
on boot
    start oppo_uefi_snapshot
    setprop sys.usb.config adb

service oppo_uefi_snapshot /system/bin/sh /system/etc/oppo-uefi-capture.sh
    disabled
    oneshot
    user root
    group root
    seclabel u:r:recovery:s0
'''
SNAPSHOT = b'''#!/system/bin/sh
umask 077
dest=/tmp/oppo-uefi
mkdir -p "$dest"
cat /sys/firmware/fdt > "$dest/runtime.dtb" 2> "$dest/fdt-error.txt"
cat /proc/iomem > "$dest/iomem.txt" 2> "$dest/iomem-error.txt"
for field in name virtual_size bits_per_pixel stride; do
    cat "/sys/class/graphics/fb0/$field" > "$dest/fb-$field.txt" 2> "$dest/fb-$field-error.txt"
done
cat /sys/class/power_supply/battery/capacity > "$dest/battery-percent.txt"
echo snapshot-service-completed > "$dest/status.txt"
# The stock fastbootd trigger uses cmode=1 to connect this MT6779 USB controller.
# Reassert ADB after the recovery UI has completed its initial USB setup.
sleep 15
echo 1 > /sys/class/udc/musb-hdrc/device/cmode
setprop sys.usb.config none
sleep 1
setprop sys.usb.config adb
echo usb-activation-requested >> "$dest/status.txt"
'''


def patch_entries(entries):
    mapping = {entry.name: entry for entry in entries}
    for name in (RC_PATH, 'secure.prop', 'prop.default', 'system/bin/adbd'):
        if name not in mapping or mapping[name].fields[1] & 0xf000 != 0x8000:
            raise ValueError('Required original regular file missing: ' + name)
    if SCRIPT_PATH in mapping or b'oppo_uefi_snapshot' in mapping[RC_PATH].data:
        raise ValueError('Refusing an already modified recovery')
    if mapping['secure.prop'].data.rstrip(b'\n').splitlines() != [b'ro.debuggable=0', b'ro.adb.secure=1', b'ro.secure=1']:
        raise ValueError('Unexpected stock recovery security properties')
    modified = {
        RC_PATH: mapping[RC_PATH].data + RC_APPEND,
        'secure.prop': b'ro.debuggable=1\nro.adb.secure=0\nro.secure=0\n\n',
    }
    result = [entry.with_data(modified[entry.name]) if entry.name in modified else entry
              for entry in entries]
    # A new inode, regular root-owned file, retaining deterministic stock time.
    fields = list(mapping[RC_PATH].fields)
    fields[0] = max(entry.fields[0] for entry in entries) + 1
    fields[1], fields[4] = 0o100644, 1
    fields[11] = len(SCRIPT_PATH.encode('utf-8')) + 1
    result.append(Entry(SCRIPT_PATH, tuple(fields), b'').with_data(SNAPSHOT))
    return result, sorted([*modified, SCRIPT_PATH])


def prepare(source, output_dir, avbtool):
    repo = Path(__file__).resolve().parents[1]
    if output_dir.resolve().is_relative_to(repo):
        raise ValueError('Firmware candidates must stay outside the public repository')
    if output_dir.exists():
        raise ValueError('Choose a fresh output directory; existing evidence is preserved')
    raw = source.read_bytes()
    if len(raw) != PARTITION_SIZE or hashlib.sha256(raw).hexdigest() != STOCK_SHA256:
        raise ValueError('Source is not the verified original recovery backup')
    original = AndroidV2(raw)
    entries = parse_newc(inflate_ramdisk(original.components['ramdisk']))
    modified, changed = patch_entries(entries)
    packed = pack_newc(modified)
    if parse_newc(packed) != modified:
        raise ValueError('Archive metadata/data round trip failed')
    ramdisk = gzip.compress(packed, compresslevel=9, mtime=0)
    candidate = original.replace_ramdisk(ramdisk)
    check = AndroidV2(candidate)
    for name in ('kernel', 'second', 'recovery_dtbo', 'dtb'):
        if check.components[name] != original.components[name]:
            raise ValueError('Unexpected modification to ' + name)
    if len(candidate) + 65536 > PARTITION_SIZE:
        raise ValueError('Candidate does not fit recovery partition with AVB metadata')
    output_dir.mkdir(parents=True)
    image = output_dir / 'recovery.img'
    image.write_bytes(candidate)
    retained = add_footer(image, raw, avbtool, PARTITION_SIZE)
    final = image.read_bytes()
    if len(final) != PARTITION_SIZE or final[-64:-60] != b'AVBf':
        raise ValueError('Candidate AVB footer/partition size validation failed')
    report = {'schema_version': 1, 'purpose': 'private-recovery-readout-candidate',
              'stock_sha256': STOCK_SHA256,
              'candidate_sha256': hashlib.sha256(final).hexdigest(),
              'candidate_size': len(final), 'changed_ramdisk_paths': changed,
              'unchanged_components': {name: hashlib.sha256(check.components[name]).hexdigest()
                                       for name in ('kernel', 'second', 'recovery_dtbo', 'dtb')},
              'avb_algorithm': 'NONE', 'oem_signature_valid': False,
              'stock_avb_properties_preserved': retained,
              'requires_already_unlocked_bootloader': True,
              'persistent_block_writes_in_added_script': False,
              'adb_authentication_in_candidate': False,
              'restore_stock_recovery_after_diagnostic': True,
              'phone_operations_performed': False, 'physical_execution_verified': False}
    (output_dir / 'build.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-recovery', type=Path, required=True)
    parser.add_argument('--private-output-dir', type=Path, required=True)
    parser.add_argument('--avbtool', type=Path, required=True)
    args = parser.parse_args()
    report = prepare(args.stock_recovery, args.private_output_dir, args.avbtool)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
