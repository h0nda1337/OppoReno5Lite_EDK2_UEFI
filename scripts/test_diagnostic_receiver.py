# SPDX-License-Identifier: BSD-2-Clause
from io import BytesIO
from types import SimpleNamespace
import unittest
import time

from receive_diagnostic import FILE_LIMIT, USB_IDENTITY, matching_port, receive


def sample_transfer():
    data = b'OPPO-DIAG/1 BEGIN\n'
    for name, value in [('runtime.dtb', b'\x00\xff\r\n\x00'), ('iomem.txt', b'ram\n'),
                        ('framebuffer.json', b'{}'), ('battery.txt', b'29\n')]:
        data += f'FILE {name} {len(value)}\n'.encode() + value + b'\n'
    return data + b'OPPO-DIAG/1 END\n'


class ReceiverTests(unittest.TestCase):
    def test_disconnected_or_dripping_stream_cannot_extend_total_deadline(self):
        class SlowBytes(BytesIO):
            def read(self, size):
                time.sleep(0.005)
                return super().read(min(size, 1))
        for stream in (BytesIO(sample_transfer()[:-5]), SlowBytes(sample_transfer())):
            start = time.monotonic()
            with self.assertRaisesRegex(ValueError, 'timed-out'):
                receive(stream, timeout_seconds=0.03)
            self.assertLess(time.monotonic() - start, 1)

    def test_binary_delimiters_are_not_interpreted_as_line_endings(self):
        received, level = receive(BytesIO(sample_transfer()))
        self.assertEqual(received['runtime.dtb'], b'\x00\xff\r\n\x00')
        self.assertEqual(level, 29)

    def test_rejects_unexpected_name_size_identity_and_low_battery(self):
        base = sample_transfer()
        for raw in (base.replace(b'runtime.dtb', b'../outside'),
                    base.replace(b'runtime.dtb 5', f'runtime.dtb {FILE_LIMIT + 1}'.encode()),
                    base.replace(b'OPPO-DIAG/1 BEGIN', b'OTHER-DIAG BEGIN'),
                    base.replace(b'29\n', b'19\n')):
            with self.assertRaises(ValueError):
                receive(BytesIO(raw))

    def test_other_phones_never_match_and_duplicate_targets_rejected(self):
        own = SimpleNamespace(vid=0x1d6b, pid=0x0104, serial_number=USB_IDENTITY)
        other = SimpleNamespace(vid=0x18d1, pid=0x4ee7, serial_number='unrelated')
        self.assertIs(matching_port([own, other]), own)
        self.assertIsNone(matching_port([other]))
        with self.assertRaises(ValueError):
            matching_port([own, own])


if __name__ == '__main__':
    unittest.main()
