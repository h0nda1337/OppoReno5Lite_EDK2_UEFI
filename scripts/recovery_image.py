#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Bounded newc archive and Android v2 image editing, entirely on the host.

Archive paths are parsed as data, never extracted to the filesystem.
"""
from dataclasses import dataclass, replace
import struct
import zlib


def align(value, boundary=4):
    return (value + boundary - 1) // boundary * boundary


@dataclass(frozen=True)
class Entry:
    name: str
    fields: tuple
    data: bytes

    def with_data(self, data):
        fields = list(self.fields)
        fields[6] = len(data)
        fields[12] = 0
        return replace(self, fields=tuple(fields), data=data)


def inflate_ramdisk(data, limit=128 * 1024**2):
    decoder = zlib.decompressobj(31)
    raw = decoder.decompress(data, limit + 1)
    if len(raw) > limit or not decoder.eof or decoder.unused_data:
        raise ValueError('Truncated, concatenated or excessive ramdisk')
    return raw


def parse_newc(raw):
    entries, names, offset = [], set(), 0
    while offset + 110 <= len(raw):
        header = raw[offset:offset + 110]
        if header[:6] != b'070701':
            raise ValueError('Only newc without checksum is supported')
        try:
            fields = tuple(int(header[i:i + 8], 16) for i in range(6, 110, 8))
        except ValueError as error:
            raise ValueError('Invalid newc numeric field') from error
        size, name_size = fields[6], fields[11]
        if not 1 <= name_size <= 4096 or offset + 110 + name_size > len(raw):
            raise ValueError('Invalid newc name bounds')
        name_raw = raw[offset + 110:offset + 110 + name_size]
        if name_raw[-1:] != b'\0' or b'\0' in name_raw[:-1]:
            raise ValueError('Invalid newc name terminator')
        name = name_raw[:-1].decode('utf-8')
        if name.startswith('/') or '..' in name.split('/') or name in names:
            raise ValueError('Unsafe or duplicate newc name')
        start = align(offset + 110 + name_size)
        end = start + size
        if end > len(raw):
            raise ValueError('Invalid newc data bounds')
        entry = Entry(name, fields, raw[start:end])
        offset = align(end)
        if name == 'TRAILER!!!':
            if size or any(raw[offset:]):
                raise ValueError('Invalid newc trailer or nonzero tail')
            return entries
        names.add(name)
        entries.append(entry)
    raise ValueError('newc trailer missing')


def pack_newc(entries):
    output = bytearray()
    names = set()
    for entry in [*entries, Entry('TRAILER!!!', (0,) * 13, b'')]:
        if entry.name in names:
            raise ValueError('Duplicate newc name')
        names.add(entry.name)
        name = entry.name.encode('utf-8') + b'\0'
        fields = list(entry.fields)
        if len(fields) != 13 or any(not 0 <= value <= 0xffffffff for value in fields):
            raise ValueError('Invalid newc metadata')
        fields[6], fields[11], fields[12] = len(entry.data), len(name), 0
        output.extend(b'070701' + ''.join(f'{value:08x}' for value in fields).encode('ascii'))
        output.extend(name)
        output.extend(bytes(align(len(output)) - len(output)))
        output.extend(entry.data)
        output.extend(bytes(align(len(output)) - len(output)))
    output.extend(bytes(align(len(output), 512) - len(output)))
    return bytes(output)


class AndroidV2:
    def __init__(self, raw):
        if len(raw) < 1660 or raw[:8] != b'ANDROID!':
            raise ValueError('Expected Android v2 boot image')
        self.raw = raw
        le = lambda offset: struct.unpack_from('<I', raw, offset)[0]
        self.page = le(36)
        if le(40) != 2 or le(1644) != 1660 or self.page not in (2048, 4096, 8192, 16384):
            raise ValueError('Unsupported Android v2 geometry')
        self.header = raw[:self.page]
        self.components = {}
        offset = self.page
        for name, field in [('kernel', 8), ('ramdisk', 16), ('second', 24),
                            ('recovery_dtbo', 1632), ('dtb', 1648)]:
            size = le(field)
            if offset + size > len(raw):
                raise ValueError('Truncated Android component: ' + name)
            if name == 'recovery_dtbo' and size:
                declared = struct.unpack_from('<Q', raw, 1636)[0]
                if declared != offset:
                    raise ValueError('Unexpected recovery DTBO offset')
            self.components[name] = raw[offset:offset + size]
            offset += align(size, self.page)
        self.content_size = offset

    def replace_ramdisk(self, ramdisk):
        import hashlib
        header = bytearray(self.header)
        struct.pack_into('<I', header, 16, len(ramdisk))
        components = dict(self.components, ramdisk=ramdisk)
        offset = self.page
        output = bytearray(header)
        for name in ('kernel', 'ramdisk', 'second', 'recovery_dtbo', 'dtb'):
            data = components[name]
            if name == 'recovery_dtbo':
                struct.pack_into('<Q', output, 1636, offset if data else 0)
            output.extend(data)
            output.extend(bytes(align(len(output), self.page) - len(output)))
            offset = len(output)
        # Android mkbootimg v2 ID: component data followed by little-endian size;
        # DTB length uses uint32, recovery DTBO length uses uint32 as well.
        digest = hashlib.sha1()
        for name in ('kernel', 'ramdisk', 'second', 'recovery_dtbo', 'dtb'):
            digest.update(components[name])
            digest.update(struct.pack('<I', len(components[name])))
        output[576:608] = digest.digest() + bytes(12)
        return bytes(output)
