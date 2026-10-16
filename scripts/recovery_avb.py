#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Retain stock AVB property descriptors in host-only recovery candidates.

Preserving properties does not preserve the OEM signature, root of trust or
physical boot behavior. Candidates remain unsigned and installation suspended.
"""
import re
import struct
import subprocess
import sys


def properties(image):
    if len(image) < 64:
        raise ValueError('Missing AVB footer')
    magic, major, minor, original, offset, size = struct.unpack_from('>4sIIQQQ', image, len(image) - 64)
    if (magic != b'AVBf' or major != 1 or minor != 0 or original > offset or
            size < 256 or offset + size > len(image) - 64):
        raise ValueError('Unsupported or out-of-bounds AVB footer')
    header = image[offset:offset + 256]
    if header[:4] != b'AVB0':
        raise ValueError('Missing vbmeta header')
    auth, aux = struct.unpack_from('>QQ', header, 12)
    desc_offset, desc_size = struct.unpack_from('>QQ', header, 96)
    if (256 + auth + aux > size or desc_offset + desc_size > aux or
            desc_size > 1024 * 1024):
        raise ValueError('Invalid AVB auxiliary/descriptor bounds')
    position = offset + 256 + auth + desc_offset
    end = position + desc_size
    result = {}
    count = 0
    while position < end:
        if position + 16 > end:
            raise ValueError('Truncated AVB descriptor')
        tag, following = struct.unpack_from('>QQ', image, position)
        total = 16 + following
        if total % 8 or total < 16 or position + total > end:
            raise ValueError('Invalid AVB descriptor size')
        if tag == 0:
            if total < 32:
                raise ValueError('Truncated AVB property descriptor')
            key_size, value_size = struct.unpack_from('>QQ', image, position + 16)
            used = 32 + key_size + 1 + value_size + 1
            if not 0 < key_size <= 256 or value_size > 4096 or (used + 7) // 8 * 8 != total:
                raise ValueError('Invalid AVB property lengths')
            key_start = position + 32
            value_start = key_start + key_size + 1
            value_end = value_start + value_size
            if image[key_start + key_size] != 0 or image[value_end] != 0 or any(image[value_end + 1:position + total]):
                raise ValueError('Invalid AVB property terminator/padding')
            key = image[key_start:key_start + key_size].decode('utf8')
            value = image[value_start:value_end].decode('utf8')
            if not re.fullmatch(r'[A-Za-z0-9_.-]+', key) or '\0' in value or key in result:
                raise ValueError('Invalid or duplicate AVB property')
            result[key] = value
        position += total
        count += 1
        if count > 1024:
            raise ValueError('Excessive AVB descriptors')
    return result


def add_footer(image, stock, avbtool, partition_size):
    retained = properties(stock)
    if not retained:
        raise ValueError('Expected reviewed stock recovery AVB properties')
    command = [sys.executable, str(avbtool), 'add_hash_footer', '--image', str(image),
               '--partition_name', 'recovery', '--partition_size', str(partition_size),
               '--algorithm', 'NONE', '--salt', '']
    for key, value in sorted(retained.items()):
        command.extend(['--prop', key + ':' + value])
    subprocess.run(command, check=True, capture_output=True)
    subprocess.run([sys.executable, str(avbtool), 'verify_image', '--image', str(image)],
                   check=True, capture_output=True)
    if properties(image.read_bytes()) != retained:
        raise ValueError('Candidate did not retain every stock AVB property')
    return retained
