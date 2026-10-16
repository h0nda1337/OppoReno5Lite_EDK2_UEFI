#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Active-session probe: temporarily boot the owner's verified stock image.

No flash/erase/lock/unlock commands exist in this script. Not for unattended use.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import time

from device_session import ReadOnlyAdb

EXPECTED_STOCK_SHA256 = '1aed5bb3e2f6ebb2b822a65499b967d2a23397d666d528a90392dcac460aaa51'
EXPECTED_RECOVERY_SHA256 = 'ac989e3009148792101a764a82fca8ca6212ec0afdd84e13d3b85e87364903b1'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--stock-image', required=True, type=Path)
    parser.add_argument('--profile', choices=('android', 'recovery'), default='android')
    parser.add_argument('--private-report', required=True, type=Path)
    parser.add_argument('--execute-stock-probe', action='store_true')
    args = parser.parse_args()
    if not args.execute_stock_probe:
        parser.error('Explicit active-session execution flag is required')
    root = Path(__file__).resolve().parents[1]
    if args.private_report.resolve().is_relative_to(root):
        parser.error('Raw probe report must remain outside the public repository')
    if args.private_report.exists():
        parser.error('Preserve prior probe reports; choose a new report path')
    digest = hashlib.sha256(args.stock_image.read_bytes()).hexdigest()
    expected = EXPECTED_STOCK_SHA256 if args.profile == 'android' else EXPECTED_RECOVERY_SHA256
    if digest != expected:
        parser.error('Stock image does not match the verified original OPPO backup')
    session = ReadOnlyAdb(args.serial)
    properties = session.properties()
    if (properties['ro.boot.flash.locked'] != '0' or
            properties['ro.build.version.ota'] != 'CPH2205_11.F.72_2720_202604151915'):
        parser.error('Unexpected bootloader state or stock firmware version')
    fastboot = shutil.which('fastboot')
    if not fastboot:
        parser.error('Fastboot unavailable')
    state = {'state': 'preflight', 'stock_image_sha256': digest,
             'profile': args.profile,
             'persistent_flash_commands_sent': False,
             'lock_unlock_commands_sent': False,
             'ram_boot_execution_verified': False, 'results': []}

    def save():
        state['battery_samples'] = session.battery_samples
        args.private_report.parent.mkdir(parents=True, exist_ok=True)
        args.private_report.write_text(json.dumps(state, indent=2), encoding='utf-8')

    def fb(*command, timeout=15):
        result = subprocess.run([fastboot, '-s', args.serial, *command],
                                capture_output=True, text=True, timeout=timeout)
        state['results'].append({'command': list(command[:2]), 'returncode': result.returncode,
                                 'output': (result.stdout + result.stderr)[:3000]})
        save()
        return result

    def getvar(name):
        result = fb('getvar', name)
        match = re.search(re.escape(name) + r':\s*([^\r\n]+)', result.stdout + result.stderr)
        if result.returncode or not match:
            raise RuntimeError('Required fastboot variable unavailable: ' + name)
        return match.group(1).strip()

    try:
        session.battery()
        state['state'] = 'entering_bootloader'
        save()
        subprocess.run([session.adb, '-s', args.serial, 'reboot', 'bootloader'],
                       check=True, timeout=15, capture_output=True)
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            devices = subprocess.run([fastboot, 'devices'], capture_output=True,
                                     text=True, timeout=10).stdout
            if any(line.split()[0] == args.serial for line in devices.splitlines() if line.split()):
                break
            time.sleep(1)
        else:
            raise RuntimeError('Target did not enumerate in bootloader fastboot')
        if getvar('is-userspace') != 'no' or getvar('unlocked') != 'yes':
            raise RuntimeError('Unexpected fastboot mode or lock state')
        if getvar('product') not in ('k79v1_64', 'OP4F43L1'):
            raise RuntimeError('Unexpected bootloader product')
        limit = getvar('max-download-size')
        if int(limit, 0) < args.stock_image.stat().st_size:
            raise RuntimeError('Stock image exceeds bootloader download capacity')
        state['state'] = 'testing_original_stock_ram_boot'
        save()
        print('Testing temporary boot with verified original stock', args.profile, 'image.', flush=True)
        result = fb('boot', str(args.stock_image), timeout=90)
        state['boot_command_accepted'] = result.returncode == 0
        print((result.stdout + result.stderr)[:3000], flush=True)
        if args.profile == 'recovery':
            state['state'] = 'awaiting_recovery_observation'
            state['observation_required'] = 'Identify recovery menu or Android; no automatic reboot sent'
            save()
            print('Recovery-profile probe finished; observe the screen before proceeding.', flush=True)
            return
        if result.returncode:
            devices = subprocess.run([fastboot, 'devices'], capture_output=True,
                                     text=True, timeout=10).stdout
            present = any(line.split()[0] == args.serial for line in devices.splitlines() if line.split())
            if present:
                reboot = fb('reboot', timeout=20)
                if reboot.returncode:
                    raise RuntimeError('Could not request return to Android')
            else:
                state['transport_disconnected_before_ack'] = True
        state['state'] = 'waiting_for_android'
        save()
        deadline = time.monotonic() + 180
        while time.monotonic() < deadline:
            result = subprocess.run([session.adb, '-s', args.serial, 'shell', 'getprop',
                                     'sys.boot_completed'], capture_output=True,
                                    text=True, timeout=10)
            if result.returncode == 0 and result.stdout.strip() == '1':
                state['android_boot_completed'] = True
                state['properties_after'] = session.properties()
                session.battery()
                state['state'] = 'complete'
                save()
                print(json.dumps({'state': state['state'],
                                  'boot_command_accepted': state['boot_command_accepted'],
                                  'android_boot_completed': True,
                                  'battery_percent': session.battery_samples[-1]['percent']}, indent=2))
                return
            time.sleep(3)
        raise RuntimeError('Android return requires screen/USB observation')
    except BaseException as error:
        state['state'] = 'needs_attention'
        state['error'] = str(error)
        save()
        raise


if __name__ == '__main__':
    main()
