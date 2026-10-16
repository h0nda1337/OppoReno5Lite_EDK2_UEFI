# SPDX-License-Identifier: BSD-2-Clause
import unittest

from recovery_diagnostic_session import partition_size, require_minimal_readout


class FastbootSizeTests(unittest.TestCase):
    def test_only_explicit_minimal_readout_can_enter_recovery(self):
        minimal = {'purpose': 'minimal-linux-readout'}
        require_minimal_readout(minimal, True, True)
        for manifest, explicit, direct in (({}, True, True),
                                           ({'purpose': 'private-recovery-readout-candidate'}, True, True),
                                           (minimal, False, True), (minimal, True, False)):
            with self.assertRaises(ValueError):
                require_minimal_readout(manifest, explicit, direct)

    def test_oem_hex_without_prefix_and_standard_prefix(self):
        for text in ('4000000', '0x4000000', '0X04000000'):
            self.assertEqual(partition_size(text), 64 * 1024**2)
        self.assertEqual(partition_size('0x8000000'), 128 * 1024**2)

    def test_invalid_values_rejected(self):
        for text in ('', '-1', '64 MB', 'unknown', '4000000 trailing'):
            with self.assertRaises(ValueError):
                partition_size(text)


if __name__ == '__main__':
    unittest.main()
