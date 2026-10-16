# SPDX-License-Identifier: BSD-2-Clause
import copy
import unittest

from analyze_diagnostic import framebuffer_observation, iomem_ranges


class DiagnosticAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.runtime = {'framebuffer_handoffs_consistent': False,
                        'framebuffer_handoffs': [
                            {'address': '0x80000000', 'size': '0x2000000', 'encoding': 'native'},
                            {'address': '0x90000000', 'size': '0x2000000', 'encoding': 'split'}],
                        'hardware': {'reserved_memory': [
                            {'path': '/reserved-memory/framebuffer',
                             'reg': [{'address': '0x80000000', 'size': '0x2000000'}]}]}}
        self.screen = {'schema_version': 1, 'source': 'linux-fb-ioctl',
                       'physical_base': '0x80000000', 'memory_size': 4 * 640 * 480,
                       'width': 640, 'height': 480, 'virtual_width': 640,
                       'virtual_height': 480, 'stride_bytes': 2560,
                       'xoffset': 0, 'yoffset': 0, 'bits_per_pixel': 32,
                       'red': [0, 8], 'green': [8, 8], 'blue': [16, 8], 'alpha': [24, 8],
                       'unexpected_private_field': 'must not be copied'}

    def test_independent_driver_and_reservation_corroborate_one_of_conflicting_tags(self):
        report = framebuffer_observation(self.runtime, self.screen)
        self.assertTrue(report['linux_driver_framebuffer_verified'])
        self.assertTrue(report['runtime_tags_disagree'])
        self.assertFalse(report['native_gop_execution_verified'])
        self.assertEqual(report['matching_runtime_tags'], ['native'])
        self.assertNotIn('unexpected_private_field', report)

    def test_rejects_uncorroborated_address_geometry_overflow_and_overlapping_channels(self):
        for key, value in [('physical_base', '0xa0000000'), ('physical_base', '0xffffffffffffffff'),
                           ('memory_size', 1024), ('yoffset', 1), ('stride_bytes', 2559),
                           ('green', [0, 8]), ('width', True)]:
            with self.subTest(key=key, value=value):
                changed = dict(self.screen, **{key: value})
                with self.assertRaises(ValueError):
                    framebuffer_observation(self.runtime, changed)
        runtime = copy.deepcopy(self.runtime)
        runtime['hardware']['reserved_memory'] = []
        with self.assertRaises(ValueError):
            framebuffer_observation(runtime, self.screen)

    def test_iomem_whitelist_excludes_arbitrary_device_labels(self):
        text = ('40000000-4fffffff : System RAM\n  40080000-400fffff : Kernel code\n'
                '  41000000-41000fff : reserved\n50000000-50000fff : private-device-name\n')
        report = iomem_ranges(text)
        self.assertEqual(len(report), 3)
        self.assertNotIn('private-device-name', str(report))
        with self.assertRaises(ValueError):
            iomem_ranges('00000000-00000000 : other\n')


if __name__ == '__main__':
    unittest.main()
