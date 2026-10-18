#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Inspect staged Git blobs for accidental binary/private publication."""
from pathlib import Path
import hashlib
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ALLOWED_ROOT_FILES = {'README.md', 'LICENSE', 'AGENTS.md', '.gitignore', '.gitattributes'}
ALLOWED_DIRS = {'docs', 'scripts', 'evidence', 'OppoReno5LitePkg', '.github'}
ALLOWED_SUFFIXES = {'.md', '.py', '.json', '.c', '.h', '.s', '.asm', '.inf',
                    '.dsc', '.fdf', '.dec', '.inc', '.yml', '.yaml', '.txt', '.patch', '.sh'}
# Owner explicitly requested these photographs. Reviewed visually; camera EXIF
# and other APP metadata removed, keeping only orientation and its ICC profile.
# Exact bytes are pinned; this is not a general permission to publish images.
REVIEWED_IMAGES = {
    'docs/images/native-mmu-probe.jpg':
        (2040440, '7f4da2bfba27abeaaf28a5d24612b3fb6cfd338e8530ca42d6af3d815bf63ff0'),
    'docs/images/native-dxe-shell-lab-01.jpg':
        (1453605, '42862c3cafbb81f1ab6790c08637d8643a9c46335552572600f0355d2f022c01'),
    'docs/images/native-dxe-shell-lab-02.jpg':
        (3231441, '3a3181c537b2c612d88285da116a65e156be76856da0309a52a5ea7f701d564d'),
    'docs/images/native-dxe-shell-lab-03.jpg':
        (2944218, '4a009f4a0abe5b781febac26175d27c7e41daf66a97f56a68e5718ca2aa4ca6c'),
    'docs/images/native-dxe-shell-lab-04.jpg':
        (2545787, '6ff6ee24d932261c47b03008fa2d9b2579b4708b2698642faddf25f16e511425'),
    'docs/images/native-dxe-shell-lab-06.jpg':
        (2628480, 'e22916db2109d93cc100f54f11fbf9ff8195ae53c261ae911aa319b529c47b1b'),
    'docs/images/native-dxe-shell-lab-06-detail.jpg':
        (2624217, 'f52b09eee2cec8daf12582c1846b80d2837640c9e9a103bd66c98d482e8d8fc4'),
    'docs/images/native-dxe-shell-lab-07.jpg':
        (2393497, 'e0cff71d2815359a2ea9165fa3de5df40a887c58dfb775e5d3ab2224baed3b46'),
    'docs/images/native-sd-read-lab-08.jpg':
        (2770182, 'c1fc73a092c17372be7d85f26a86ec079d250abeec8afdaae38e180cc42741f5'),
    'docs/images/native-sd-read-lab-08-context.jpg':
        (1252223, '9b5598bfe603e79a6ac2e9ea899ae0c94b0e91db162ade43297bdbaac8d01258'),
    'docs/images/native-sd-read-lab-09.jpg':
        (2255958, 'd11d5f34a509147574b817ea60f290d5fb639a8f41827bce4faddac13097d899'),
    'docs/images/native-sd-read-lab-10.jpg':
        (2505701, '84c87c338d7c15fa9676cf6e16589719a9ca169a3f6336e8a88ce75832c05b08'),
    'docs/images/native-sd-read-lab-11.jpg':
        (2542960, 'd8779f93fb7dc3edc450f3092af21729630529a8c804d9361fa7c543d5a2fe3d'),
    'docs/images/native-sd-read-lab-12.jpg':
        (2948221, 'd403e777eda20a481341a24abb79ec9a3b6464a0581409fb216e25b6e4f39cfa'),
    'docs/images/native-sd-read-lab-13.jpg':
        (2511870, 'dc9eeeb6ef2950bcfae75b7a0ef02c321a8dab56639c14cf7fedb1e6266f9a2a'),
    'docs/images/native-sd-read-lab-14.jpg':
        (2751510, '83a88ee19b7c74c4dea9d2af5c9b03847b4d2a374e6f6940a094ea915314efef'),
    'docs/images/native-sd-power-lab-16.jpg':
        (2800058, 'ccd254bfddc00db6459ba582358b46f0be81d35161e20c86ef848895ea2a9fbd'),
}
PRIVATE = [
    re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY-----'),
    re.compile(rb'gh[pousr]_[A-Za-z0-9]{30,}'),
    re.compile(rb'github_pat_[A-Za-z0-9_]{40,}'),
    re.compile(rb'(?im)^\s*(?:ro\.serialno|ro\.boot\.serialno|androidboot\.serialno|imei[12]?)\s*[=:]\s*\S+'),
]


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def validate(name, data):
    if name in REVIEWED_IMAGES:
        size, expected = REVIEWED_IMAGES[name]
        if len(data) != size or hashlib.sha256(data).hexdigest() != expected:
            raise ValueError('Reviewed image changed; inspect it again: ' + name)
        return
    path = Path(name)
    if name not in ALLOWED_ROOT_FILES:
        if len(path.parts) < 2 or path.parts[0] not in ALLOWED_DIRS:
            raise ValueError('Path requires publication review: ' + name)
        if path.suffix.lower() not in ALLOWED_SUFFIXES:
            raise ValueError('Unexpected publication file type: ' + name)
    if len(data) > 512 * 1024 or b'\0' in data:
        raise ValueError('Large/binary file requires separate review: ' + name)
    data.decode('utf-8')
    if any(pattern.search(data) for pattern in PRIVATE):
        raise ValueError('Potential private material: ' + name)


def main():
    entries = git('diff', '--cached', '--name-only', '-z', '--diff-filter=ACMR')
    names = [name.decode('utf-8') for name in entries.split(b'\0') if name]
    for name in names:
        validate(name, git('show', ':' + name))
    print('Publication check passed for', len(names), 'staged files.')


if __name__ == '__main__':
    main()
