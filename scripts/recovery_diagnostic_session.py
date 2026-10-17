#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Active-session recovery-only diagnostic installation or stock restoration.

Requires the verified original backup, explicit target and execution flag.
Never changes boot, LK, preloader or lock state. Android may restore stock
recovery automatically; use direct recovery entry when testing the candidate.
Not for unattended automation. Keep the stock backup available until restored.

OEM-ramdisk diagnostics remain suspended after the encryption-key incident.
An explicitly selected minimal Linux readout may run after candidate validation;
it contains no OEM recovery service or persistent-filesystem mounts.
A separate, explicit short native SEC entry probe may run with an empty ramdisk.
It requests reset after ten seconds and does not enter DXE or the Shell.
See docs/RECOVERY_DIAGNOSTICS.md.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time

from device_session import ReadOnlyAdb
from prepare_recovery_diagnostic import PARTITION_SIZE, STOCK_SHA256, patch_entries
from prepare_minimal_diagnostic import minimal_entries
from recovery_image import AndroidV2, inflate_ramdisk, parse_newc
from recovery_avb import properties as avb_properties
from prepare_native_probe import validate_candidate as validate_native_probe

def require_minimal_readout(manifest, explicit_minimal, direct_entry):
    if (manifest.get('purpose') != 'minimal-linux-readout' or
            not explicit_minimal or not direct_entry):
        raise ValueError('Only an explicit minimal Linux readout with direct entry is permitted; OEM-ramdisk diagnostics remain suspended')


def require_native_probe(manifest, explicit_native, direct_entry):
    if (manifest.get('purpose') != 'native-sec-entry-probe' or not explicit_native or
            not direct_entry or manifest.get('probe_only') is not True or
            manifest.get('reset_request_after_seconds') != 10):
        raise ValueError('Only the explicit short native SEC probe may use this phase')


def partition_size(value):
    # Fastboot partition-size is hexadecimal, including OEM values without 0x.
    if not re.fullmatch(r'(?:0[xX])?[0-9a-fA-F]+', value):
        raise ValueError('Invalid hexadecimal fastboot partition size')
    return int(value, 16)


def validate_candidate(stock, candidate, manifest):
    if hashlib.sha256(candidate).hexdigest() != manifest['candidate_sha256']:
        raise ValueError('Candidate does not match reviewed build manifest')
    if len(candidate) != PARTITION_SIZE or candidate[-64:-60] != b'AVBf':
        raise ValueError('Candidate size/footer mismatch')
    if avb_properties(candidate) != avb_properties(stock):
        raise ValueError('Candidate AVB properties differ from original recovery')
    original, changed = AndroidV2(stock), AndroidV2(candidate)
    if any(original.components[name] != changed.components[name]
           for name in ('kernel', 'second', 'recovery_dtbo', 'dtb')):
        raise ValueError('Candidate changed an original non-ramdisk component')
    actual = parse_newc(inflate_ramdisk(changed.components['ramdisk']))
    if manifest.get('purpose') == 'minimal-linux-readout':
        binaries = [entry.data for entry in actual if entry.name == 'init']
        if len(binaries) != 1 or hashlib.sha256(binaries[0]).hexdigest() != manifest['init_sha256']:
            raise ValueError('Minimal init does not match build manifest')
        source = Path(__file__).with_name('capture_init.c')
        if hashlib.sha256(source.read_bytes()).hexdigest() != manifest['init_source_sha256']:
            raise ValueError('Minimal diagnostic source changed since its build')
        expected = minimal_entries(binaries[0])
    else:
        expected, _ = patch_entries(parse_newc(inflate_ramdisk(original.components['ramdisk'])))
    if actual != expected:
        raise ValueError('Candidate has unexpected ramdisk changes')
    prefix = original.replace_ramdisk(changed.components['ramdisk'])
    if candidate[:len(prefix)] != prefix:
        raise ValueError('Candidate has unexpected header or content changes')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--stock-recovery', type=Path, required=True)
    parser.add_argument('--candidate-dir', type=Path)
    parser.add_argument('--restore-stock', action='store_true')
    parser.add_argument('--boot-recovery-direct', action='store_true')
    parser.add_argument('--execute-minimal-linux-readout', action='store_true')
    parser.add_argument('--execute-native-entry-probe', action='store_true')
    parser.add_argument('--private-report', type=Path, required=True)
    parser.add_argument('--execute-recovery-phase', action='store_true')
    args = parser.parse_args()
    if not args.execute_recovery_phase or (not args.restore_stock and not args.candidate_dir):
        parser.error('Explicit execution flag and candidate directory or restore mode required')
    if args.restore_stock and args.boot_recovery_direct:
        parser.error('Stock restoration returns to Android')
    if args.execute_native_entry_probe and (args.restore_stock or args.execute_minimal_linux_readout):
        parser.error('Native probe and Linux/restoration modes must be selected separately')
    repo = Path(__file__).resolve().parents[1]
    if args.private_report.exists() or args.private_report.resolve().is_relative_to(repo):
        parser.error('Choose a new report path outside the public repository')
    stock = args.stock_recovery.read_bytes()
    if len(stock) != PARTITION_SIZE or hashlib.sha256(stock).hexdigest() != STOCK_SHA256:
        parser.error('Original recovery backup does not match the verified device backup')
    if args.restore_stock:
        image = args.stock_recovery
    else:
        image = args.candidate_dir / 'recovery.img'
        manifest = json.loads((args.candidate_dir / 'build.json').read_text())
        if args.execute_native_entry_probe:
            require_native_probe(manifest, True, args.boot_recovery_direct)
            validate_native_probe(stock, image.read_bytes(), manifest)
        else:
            require_minimal_readout(manifest, args.execute_minimal_linux_readout,
                                    args.boot_recovery_direct)
            validate_candidate(stock, image.read_bytes(), manifest)
    session = ReadOnlyAdb(args.serial)
    operation = ('restore-stock' if args.restore_stock else
                 'native-sec-entry-probe' if args.execute_native_entry_probe else 'minimal-linux-readout')
    state = {'phase': 'preflight', 'operation': operation,
             'image_sha256': hashlib.sha256(image.read_bytes()).hexdigest(),
             'partition_target': 'recovery', 'other_partitions_written': False,
             'lock_unlock_commands_sent': False, 'results': []}

    def save():
        state['battery_samples'] = session.battery_samples
        args.private_report.parent.mkdir(parents=True, exist_ok=True)
        args.private_report.write_text(json.dumps(state, indent=2) + '\n', encoding='utf-8')

    def present():
        devices = subprocess.run(['fastboot', 'devices'], capture_output=True,
                                 text=True, timeout=10).stdout
        return any(line.split()[0] == args.serial for line in devices.splitlines() if line.split())

    def fb(*command, timeout=20):
        result = subprocess.run(['fastboot', '-s', args.serial, *command],
                                capture_output=True, text=True, timeout=timeout)
        output = result.stdout + result.stderr
        state['results'].append({'command': list(command), 'returncode': result.returncode,
                                 'output': output[:4000]})
        save()
        print(output.strip(), flush=True)
        if result.returncode:
            raise RuntimeError('Fastboot command failed: ' + command[0])
        return output

    def getvar(key):
        output = fb('getvar', key)
        match = re.search(re.escape(key) + r':\s*([^\r\n]+)', output)
        if not match:
            raise RuntimeError('Required fastboot variable missing: ' + key)
        return match.group(1).strip()

    try:
        props = session.properties()
        if props['ro.boot.flash.locked'] != '0':
            raise RuntimeError('Requires an already unlocked bootloader; no unlock will be sent')
        state['preflight_properties'] = props
        session.battery()
        state['phase'] = 'entering_bootloader'
        save()
        start = time.monotonic()
        subprocess.run([session.adb, '-s', args.serial, 'reboot', 'bootloader'],
                       check=True, capture_output=True, timeout=15)
        while time.monotonic() - start < 60:
            if present():
                break
            time.sleep(1)
        else:
            raise RuntimeError('Explicit target did not enumerate in bootloader')
        if (getvar('is-userspace') != 'no' or getvar('unlocked') != 'yes' or
                getvar('product') != 'k79v1_64'):
            raise RuntimeError('Unexpected target, mode or lock state')
        if partition_size(getvar('partition-size:recovery')) != PARTITION_SIZE:
            raise RuntimeError('Unexpected recovery partition size')
        if partition_size(getvar('max-download-size')) < PARTITION_SIZE:
            raise RuntimeError('Insufficient fastboot download capacity')
        if time.monotonic() - start > 120:
            raise RuntimeError('Preflight battery sample is too old; return to Android')
        state['phase'] = 'writing_recovery_only'
        save()
        fb('flash', 'recovery', str(image), timeout=90)
        state['flash_acknowledged'] = True
        if args.boot_recovery_direct:
            state['phase'] = 'direct_recovery_entry_requested'
            save()
            fb('reboot', 'recovery')
            state['phase'] = 'awaiting_recovery_observation'
            save()
            print('Direct recovery entry requested; no Android intermediate boot.', flush=True)
            return
        state['phase'] = 'returning_to_android'
        save()
        fb('reboot')
        deadline = time.monotonic() + 180
        while time.monotonic() < deadline:
            result = subprocess.run([session.adb, '-s', args.serial, 'shell', 'getprop',
                                     'sys.boot_completed'], capture_output=True,
                                    text=True, timeout=10)
            if result.returncode == 0 and result.stdout.strip() == '1':
                break
            time.sleep(3)
        else:
            raise RuntimeError('Observe screen/USB before further operations')
        state['properties_after'] = session.properties()
        session.battery()
        state['phase'] = 'android_boot_verified'
        save()
        print(json.dumps({'phase': state['phase'], 'operation': state['operation'],
                          'battery_percent': session.battery_samples[-1]['percent']}, indent=2))
    except BaseException as error:
        state['error'] = str(error)
        state['phone_operations_stopped'] = True
        save()
        # A failed identity or battery guard must not trigger another operation.
        # Observe the current mode before coordinating any recovery action.
        raise


if __name__ == '__main__':
    main()
