# SPDX-License-Identifier: BSD-2-Clause
"""Synthetic container corruption checks; fixtures are not executable firmware."""
import struct
import unittest
from uuid import UUID

from inspect_native_firmware import (FD_BASE, FD_SIZE, FV_START, REQUIRED, inspect,
    ENTRY_PROBE_MARKER, MMU_PROBE_MARKER, NATIVE_LAB_MARKER, NATIVE_MEMORY_LAB_MARKER, ACPI_PROFILE_LAB_MARKER, ACPI_INSTALL_LAB_MARKER, PCD_DRIVER_GUID,
    require_entry_probe, require_native_lab)


def pe_fixture():
    data = bytearray(128)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 64)
    data[64:68] = b'PE\0\0'
    struct.pack_into('<H', data, 68, 0xaa64)
    return data


def file_fixture(identifier, kind, section_type, payload, depex=None):
    section = (len(payload) + 4).to_bytes(3, 'little') + bytes([section_type]) + payload
    if depex is not None:
        section += b'\0' * (-len(section) % 4)
        section += (len(depex) + 4).to_bytes(3, 'little') + b'\x13' + depex
    header = bytearray(24)
    header[:16] = UUID(identifier).bytes_le
    header[17], header[18], header[23] = 0xaa, kind, 0xf8
    header[20:23] = (len(section) + 24).to_bytes(3, 'little')
    header[16] = (-(sum(header) - header[17] - header[23])) & 0xff
    return header + section


def fixture(probe=False, mmu=False, lab=False, pcd_depex=b'\x06\x08', pcd_present=True,
            lab_marker=NATIVE_LAB_MARKER, sd=False):
    raw = bytearray(b'\xff' * FD_SIZE)
    raw[:64] = b'\0' * 64
    struct.pack_into('<IIQQ', raw, 0, 0x14000400, 0xd503201f, 0x80000, FD_SIZE)
    raw[56:60] = b'ARM\x64'
    header = bytearray(72)
    header[16:32] = UUID('8c8ce578-8a3d-4f1c-9935-896185c32dd3').bytes_le
    struct.pack_into('<Q', header, 32, FD_SIZE - FV_START)
    header[40:44] = b'_FVH'
    struct.pack_into('<H', header, 48, len(header))
    header[55] = 2
    struct.pack_into('<II', header, 56, (FD_SIZE - FV_START) // 4096, 4096)
    position = FV_START + len(header)
    te_at = position + 24 + 4
    te = bytearray(44) + (ENTRY_PROBE_MARKER if probe else b'') + (MMU_PROBE_MARKER if mmu else b'')
    struct.pack_into('<HHBBHIIQ', te, 0, 0x5a56, 0xaa64, 0, 10, 40, 40, 40, FD_BASE + te_at)
    struct.pack_into('<I', te, 40, 0xd503201f)
    sec_entry = te_at + 40
    struct.pack_into('<I', header, 0, 0x14000000 | ((sec_entry - FV_START) // 4))
    checksum = (-sum(struct.unpack('<36H', header))) & 0xffff
    struct.pack_into('<H', header, 50, checksum)
    raw[FV_START:FV_START + len(header)] = header
    for identifier, (_, kind) in REQUIRED.items():
        pe = pe_fixture() + (lab_marker if lab and REQUIRED[identifier][0]=='original-bds' else b'')
        data = file_fixture(identifier, kind, 0x12 if kind == 3 else 0x10,
                            te if kind == 3 else pe)
        raw[position:position + len(data)] = data
        position = (position + len(data) + 7) & ~7
    data = file_fixture('d6a2cb7f-6a18-4e2f-b43b-9920a733700a', 5, 0x10, pe_fixture())
    raw[position:position + len(data)] = data
    position = (position + len(data) + 7) & ~7
    if pcd_present:
        data = file_fixture(PCD_DRIVER_GUID, 7, 0x10, pe_fixture(), pcd_depex)
        raw[position:position + len(data)] = data
        position = (position + len(data) + 7) & ~7
    if sd:
        data = file_fixture('056d7218-2168-44b9-9784-a28349858cc2', 7, 0x10, pe_fixture())
        raw[position:position + len(data)] = data
    return raw


class NativeFirmwareTests(unittest.TestCase):
    def test_full_integration_firmware_cannot_pass_short_probe_guard(self):
        with self.assertRaises(ValueError):
            require_entry_probe(fixture())
        self.assertFalse(require_entry_probe(fixture(probe=True))['uefi_execution_observed'])

    def test_handoff_and_mmu_probe_markers_are_not_interchangeable(self):
        require_entry_probe(fixture(mmu=True), 'mmu')
        for raw,phase in [(fixture(probe=True),'mmu'), (fixture(mmu=True),'handoff'),
                          (fixture(probe=True,mmu=True),'handoff'), (fixture(probe=True,mmu=True),'mmu'),
                          (fixture(),'mmu'), (fixture(mmu=True),'unknown')]:
            with self.assertRaises(ValueError): require_entry_probe(raw,phase)

    def test_dxe_lab_requires_its_bds_marker_and_rejects_sec_probes(self):
        self.assertFalse(require_native_lab(fixture(lab=True))['uefi_execution_observed'])
        for raw in [fixture(),fixture(probe=True,lab=True),fixture(mmu=True,lab=True)]:
            with self.assertRaises(ValueError): require_native_lab(raw)
        for phase in ['handoff','mmu']:
            with self.assertRaises(ValueError): require_entry_probe(fixture(lab=True),phase)

    def test_direct_entry_and_required_roles(self):
        result = inspect(fixture())
        self.assertTrue(result['direct_header_fv_sec_branch_verified'])
        self.assertEqual(len(result['files']), 7)
        self.assertFalse(result['uefi_execution_observed'])
        self.assertFalse(result['bootshim_used'])

    def test_exactly_one_deadline_is_extracted_from_bds(self):
        for marker, seconds in [(NATIVE_LAB_MARKER,90),(NATIVE_MEMORY_LAB_MARKER,1800)]:
            self.assertEqual(require_native_lab(fixture(lab=True,lab_marker=marker))['reset_request_after_seconds'],seconds)
        for marker in [NATIVE_LAB_MARKER+NATIVE_MEMORY_LAB_MARKER,NATIVE_MEMORY_LAB_MARKER*2,b'unknown']:
            with self.assertRaises(ValueError): require_native_lab(fixture(lab=True,lab_marker=marker))

    def test_acpi_profile_marker_excludes_storage_and_long_deadlines(self):
        marker = NATIVE_LAB_MARKER+ACPI_PROFILE_LAB_MARKER
        self.assertTrue(require_native_lab(fixture(lab=True,lab_marker=marker))['acpi_profile_read_only_lab'])
        self.assertFalse(require_native_lab(fixture(lab=True))['acpi_profile_read_only_lab'])
        for raw in (fixture(lab=True,lab_marker=marker,sd=True),
                    fixture(lab=True,lab_marker=NATIVE_MEMORY_LAB_MARKER+ACPI_PROFILE_LAB_MARKER),
                    fixture(lab=True,lab_marker=marker+ACPI_PROFILE_LAB_MARKER)):
            with self.assertRaises(ValueError): require_native_lab(raw)

    def test_acpi_installation_needs_profile_without_storage(self):
        marker=NATIVE_LAB_MARKER+ACPI_PROFILE_LAB_MARKER+ACPI_INSTALL_LAB_MARKER
        r=require_native_lab(fixture(lab=True,lab_marker=marker))
        self.assertTrue(r['acpi_install_no_os_lab'])
        self.assertFalse(r['acpi_profile_read_only_lab'])
        for raw in (fixture(lab=True,lab_marker=NATIVE_LAB_MARKER+ACPI_INSTALL_LAB_MARKER),
                    fixture(lab=True,lab_marker=marker+ACPI_INSTALL_LAB_MARKER),
                    fixture(lab=True,lab_marker=marker,sd=True)):
            with self.assertRaises(ValueError):require_native_lab(raw)

    def test_lab_rejects_pcd_self_dependency_and_missing_bootstrap(self):
        circular = b'\x02' + UUID('13a3f0f6-264a-3ef0-f2e0-dec512342f34').bytes_le + b'\x08'
        for expression in (circular, b'\x07\x08', b'\x06', b'\x06\x08\x00', None):
            with self.subTest(expression=expression):
                with self.assertRaises(ValueError):
                    require_native_lab(fixture(lab=True, pcd_depex=expression))
        with self.assertRaises(ValueError):
            require_native_lab(fixture(lab=True, pcd_present=False))
        self.assertTrue(require_native_lab(fixture(lab=True))['pcd_bootstrap_unconditional_depex_verified'])

    def test_corrupt_header_checksum_file_bounds_and_sec_entry_are_rejected(self):
        original = fixture()
        reference = inspect(original)
        sec = next(item for item in reference['files'] if item.get('role') == 'original-sec')
        te = sec['sections'][0]['offset']
        for offset, data in ((0, b'\0' * 4), (FV_START + 56, b'\0'),
                             (sec['offset'] + 20, b'\xfe\xff\xff'),
                             (te + 8, struct.pack('<I', 0xffffffff))):
            with self.subTest(offset=offset):
                raw = original.copy()
                raw[offset:offset + len(data)] = data
                with self.assertRaises(ValueError):
                    inspect(raw)

    def test_truncated_container_rejected(self):
        with self.assertRaises(ValueError):
            inspect(fixture()[:-1])


if __name__ == '__main__':
    unittest.main()
