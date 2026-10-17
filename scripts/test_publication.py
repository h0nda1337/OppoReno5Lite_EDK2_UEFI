# SPDX-License-Identifier: BSD-2-Clause
import unittest

from check_publication import ROOT, REVIEWED_IMAGES, validate


class PublicationTests(unittest.TestCase):
    def test_only_exact_reviewed_photo_is_accepted(self):
        name = 'docs/images/native-mmu-probe.jpg'
        photo = (ROOT/name).read_bytes()
        validate(name, photo)
        with self.assertRaises(ValueError):
            validate(name, photo[:-1])
        changed = bytearray(photo)
        changed[len(changed)//2] ^= 1
        with self.assertRaises(ValueError):
            validate(name, changed)
        with self.assertRaises(ValueError):
            validate('docs/images/unreviewed.jpg', photo)

    def test_all_reviewed_images_are_pinned(self):
        for name in REVIEWED_IMAGES:
            photo = (ROOT/name).read_bytes()
            validate(name, photo)
            with self.assertRaises(ValueError):
                validate(name, photo+b'additional metadata')

    def test_text_and_private_material_guards_remain_active(self):
        validate('docs/example.md', b'Reviewed public text\n')
        for name, data in [('evidence/partition.bin', b'firmware'),
                           ('docs/example.md', b'ro.serialno=private-example\n'),
                           ('docs/example.md', b'\0binary')]:
            with self.assertRaises(ValueError):
                validate(name, data)


if __name__ == '__main__':
    unittest.main()
