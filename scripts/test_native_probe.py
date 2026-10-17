# SPDX-License-Identifier: BSD-2-Clause
import gzip
import hashlib
import struct
import unittest

from prepare_native_probe import FD_SIZE, inflate_native_kernel, probe_ramdisk, repack
from recovery_image import AndroidV2, align, parse_newc
from recovery_diagnostic_session import require_native_probe, require_minimal_readout
from prepare_native_lab import require_lab_manifest


def stock_fixture():
    raw = bytearray(2048)
    raw[:8] = b'ANDROID!'
    for offset, value in [(36,2048),(40,2),(1644,1660)]:
        struct.pack_into('<I',raw,offset,value)
    for name,field,data in [('kernel',8,b'old-kernel'),('ramdisk',16,b'old-oem-ramdisk'),
                            ('second',24,b'second'),('recovery_dtbo',1632,b'dtbo'),('dtb',1648,b'dtb')]:
        struct.pack_into('<I',raw,field,len(data))
        if name=='recovery_dtbo':struct.pack_into('<Q',raw,1636,len(raw))
        raw.extend(data)
        raw.extend(bytes(align(len(raw),2048)-len(raw)))
    return bytes(raw)


class NativeProbeTests(unittest.TestCase):
    def test_only_kernel_ramdisk_sizes_id_and_dtbo_offset_change(self):
        stock = stock_fixture()
        fd = (b'native-fd' * 1024).ljust(FD_SIZE, b'\xff')
        output = repack(stock,fd)
        original, changed = AndroidV2(stock), AndroidV2(output)
        self.assertEqual(inflate_native_kernel(changed.components['kernel']),fd)
        self.assertEqual(changed.components['kernel'],gzip.compress(fd,compresslevel=9,mtime=0))
        self.assertEqual(struct.unpack_from('<I',output,8)[0],len(changed.components['kernel']))
        self.assertEqual(changed.components['ramdisk'],probe_ramdisk())
        for name in ['second','recovery_dtbo','dtb']:
            self.assertEqual(original.components[name],changed.components[name])
        old_header,new_header=bytearray(original.header),bytearray(changed.header)
        for start,end in [(8,12),(16,20),(576,608),(1636,1644)]:
            old_header[start:end]=new_header[start:end]=bytes(end-start)
        self.assertEqual(old_header,new_header)
        digest=hashlib.sha1()
        for name in ['kernel','ramdisk','second','recovery_dtbo','dtb']:
            data=changed.components[name]
            digest.update(data);digest.update(struct.pack('<I',len(data)))
        self.assertEqual(output[576:608],digest.digest()+bytes(12))

    def test_empty_archive_contains_no_oem_program(self):
        self.assertEqual(parse_newc(gzip.decompress(probe_ramdisk())),[])

    def test_gzip_transport_rejects_raw_truncated_corrupt_or_trailing_data(self):
        packed = gzip.compress(bytes(FD_SIZE), mtime=0)
        corrupt = packed[:-8] + bytes([packed[-8] ^ 1]) + packed[-7:]
        for kernel in [bytes(FD_SIZE), packed[:-1], corrupt, packed+b'\0',
                       packed+gzip.compress(b'extra',mtime=0)]:
            with self.subTest(size=len(kernel)):
                with self.assertRaises(ValueError):inflate_native_kernel(kernel)

    def test_native_gzip_has_exact_decompressed_size_and_a_bounded_output(self):
        for size in [FD_SIZE-1,FD_SIZE+1,FD_SIZE*2]:
            with self.subTest(size=size):
                with self.assertRaises(ValueError):
                    inflate_native_kernel(gzip.compress(bytes(size),mtime=0))

    def test_native_and_linux_execution_guards_remain_separate(self):
        manifest={'purpose':'native-sec-entry-probe','probe_only':True,'reset_request_after_seconds':10,'probe_stage':'handoff'}
        require_native_probe(manifest,True,True)
        require_native_probe(dict(manifest,probe_stage='mmu'),True,True)
        for explicit,direct in [(False,True),(True,False),(False,False)]:
            with self.assertRaises(ValueError):require_native_probe(manifest,explicit,direct)
        for altered in [dict(manifest,probe_only=False),dict(manifest,reset_request_after_seconds=90),
                        dict(manifest,probe_stage='dxe'),dict(manifest,probe_stage=None),
                        {'purpose':'minimal-linux-readout'}]:
            with self.assertRaises(ValueError):require_native_probe(altered,True,True)
        with self.assertRaises(ValueError):require_minimal_readout(manifest,True,True)

    def test_dxe_shell_lab_requires_a_separate_explicit_mode(self):
        manifest={'purpose':'native-dxe-shell-lab','probe_only':False,'reset_request_after_seconds':90,
                  'kernel_transport':'gzip','bootshim_used':False}
        require_lab_manifest(manifest,True,True)
        for explicit,direct in [(False,True),(True,False),(False,False)]:
            with self.assertRaises(ValueError):require_lab_manifest(manifest,explicit,direct)
        for changed in [dict(manifest,probe_only=True),dict(manifest,reset_request_after_seconds=10),
                        dict(manifest,kernel_transport='raw'),dict(manifest,bootshim_used=True)]:
            with self.assertRaises(ValueError):require_lab_manifest(changed,True,True)
        with self.assertRaises(ValueError):require_native_probe(manifest,True,True)
        with self.assertRaises(ValueError):require_minimal_readout(manifest,True,True)


if __name__=='__main__':unittest.main()
