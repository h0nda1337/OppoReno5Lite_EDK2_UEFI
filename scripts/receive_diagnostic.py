#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Receive private hardware observations from the explicitly identified USB gadget.

Only the project's CDC-ACM identity is opened. No ADB, Fastboot or block writes.
Raw DTB and iomem stay outside the public repository.
"""
import argparse
import hashlib
import json
from pathlib import Path
import time

from inspect_runtime import inspect

USB_IDENTITY = 'OPPO-UEFI-DIAG'
NAMES = ('runtime.dtb', 'iomem.txt', 'framebuffer.json', 'battery.txt')
FILE_LIMIT = 2 * 1024**2


def read_exact(stream, size, deadline):
    data = bytearray()
    while len(data) < size:
        if time.monotonic() >= deadline:
            raise ValueError('Truncated or timed-out diagnostic transfer')
        chunk = stream.read(size - len(data))
        data.extend(chunk)
        if not chunk:
            time.sleep(0.005)
    return bytes(data)


def read_line(stream, deadline):
    data = bytearray()
    while len(data) < 128:
        value = read_exact(stream, 1, deadline)
        data.extend(value)
        if value == b'\n':
            return bytes(data)
    raise ValueError('Excessive protocol line')


def receive(stream, timeout_seconds=120):
    if not 0 < timeout_seconds <= 300:
        raise ValueError('Invalid total transfer timeout')
    deadline = time.monotonic() + timeout_seconds
    if read_line(stream, deadline) != b'OPPO-DIAG/1 BEGIN\n':
        raise ValueError('Unexpected diagnostic protocol identity')
    received = {}
    for name in NAMES:
        line = read_line(stream, deadline).decode('ascii').strip().split()
        if len(line) != 3 or line[:2] != ['FILE', name] or not line[2].isdigit():
            raise ValueError('Unexpected diagnostic file header')
        size = int(line[2])
        if size > FILE_LIMIT:
            raise ValueError('Excessive diagnostic file size')
        received[name] = read_exact(stream, size, deadline)
        if read_exact(stream, 1, deadline) != b'\n':
            raise ValueError('Invalid diagnostic file delimiter')
    if read_line(stream, deadline) != b'OPPO-DIAG/1 END\n':
        raise ValueError('Missing diagnostic completion marker')
    level = int(received['battery.txt'].strip())
    if not 20 <= level <= 100:
        raise ValueError('Diagnostic battery sample below minimum or invalid')
    return received, level


def matching_port(ports):
    found = [port for port in ports if port.vid == 0x1d6b and port.pid == 0x0104
             and port.serial_number == USB_IDENTITY]
    if len(found) > 1:
        raise ValueError('Multiple matching diagnostic gadgets; target is ambiguous')
    return found[0] if found else None


def main():
    import serial
    from serial.tools import list_ports
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--private-dir', type=Path, required=True)
    parser.add_argument('--wait-seconds', type=int, default=180)
    parser.add_argument('--return-android', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if args.private_dir.exists() or args.private_dir.resolve().is_relative_to(root):
        parser.error('Choose a fresh private capture directory outside the public repository')
    args.private_dir.mkdir(parents=True)
    state = {'phase': 'waiting_usb_gadget', 'factory_identifiers_collected': False,
             'linux_diagnostic_execution_verified': False, 'uefi_execution_observed': False}

    def save():
        (args.private_dir / 'capture.json').write_text(json.dumps(state, indent=2) + '\n', encoding='utf-8')

    save()
    print('Waiting for the dedicated OPPO Linux diagnostic USB gadget.', flush=True)
    try:
        deadline = time.monotonic() + args.wait_seconds
        while time.monotonic() < deadline:
            port = matching_port(list_ports.comports())
            if port:
                break
            time.sleep(1)
        else:
            raise RuntimeError('Diagnostic USB gadget did not appear')
        with serial.Serial(port.device, 115200, timeout=1, write_timeout=5) as connection:
            connection.reset_input_buffer()
            connection.write(b'READ\n')
            received, level = receive(connection)
            for name, data in received.items():
                (args.private_dir / name).write_bytes(data)
            runtime = inspect(received['runtime.dtb'])
            compatible = runtime['hardware']['compatible']
            if not any('mt6779' in value.lower() for value in compatible):
                raise ValueError('Runtime DTB is not the expected MT6779 platform')
            (args.private_dir / 'filtered-runtime.json').write_text(
                json.dumps(runtime, indent=2) + '\n', encoding='utf-8')
            state.update(phase='capture_complete', battery_percent=level,
                         linux_diagnostic_execution_verified=True,
                         files={name: {'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                                for name, data in received.items()})
            save()
            if args.return_android:
                connection.write(b'REBOOT\n')
                state['return_to_android_requested'] = True
                save()
        print(json.dumps({'phase': state['phase'], 'battery_percent': level,
                          'memory_nodes': len(runtime['hardware']['memory_nodes']),
                          'framebuffer_handoffs': len(runtime['framebuffer_handoffs']),
                          'uefi_execution_observed': False}, indent=2), flush=True)
    except BaseException as error:
        state['phase'] = 'needs_attention'
        state['error'] = str(error)
        save()
        raise


if __name__ == '__main__':
    main()
