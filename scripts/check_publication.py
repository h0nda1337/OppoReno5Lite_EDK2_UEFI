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
    'docs/images/native-sd-boot-lab-17.jpg':
        (2790652, '92702cfd60bf5b1de3b632536b8b76cd2bfc65dd7394dc7b693e4734e296f37e'),
    'docs/images/native-sd-boot-lab-19.jpg':
        (613722, '09a9e7900793948300a3e00932e697f0f4b1c76f36027cabfbf0db83668459ef'),
    'docs/images/native-sd-boot-lab-20-reset.jpg':
        (3333533, '44b039d5daa764a4b9a26d608bbed7416f193ca8fb79fb6c1a242c93e2a928b8'),
    'docs/images/native-sd-boot-lab-20-context.jpg':
        (3098564, 'a0d2019c618cc153d00a175b0de57a5edd7a74c2e762b4dc1c5c47817b299e44'),
    'docs/images/native-sd-boot-lab-21-reset.jpg':
        (3081458, '4949e9db3a6871e689663607b9796dbd6f5a865e4ae399491e551c0b15a76b58'),
    'docs/images/native-sd-boot-lab-21-context.jpg':
        (4448260, '08b208f6a0ce20d815d0c4ee48275a81ed6fcff9d683860336026440474317cb'),
    'docs/images/native-sd-fast-read-lab-24.jpg':
        (2990554, 'f5986632f5208c358a725ac0bf91b31e289c9635930eafcb9830cb8b625f2cd9'),
    'docs/images/native-sd-fast-boot-lab-25-context.jpg':
        (2175806, 'bcd037a1950ed57ace0779ee2e6a8f91666adf4a9ecbd2bde244c2e31976224c'),
    'docs/images/native-sd-fast-boot-lab-25-error.jpg':
        (1970667, '67537688219648a5904ef8ccb93c64608c6d93c787b12925abea0e09795def91'),
    'docs/images/native-sd-reset-trace-lab-26-reset.jpg':
        (2088478, '15abb661540fad4793897b4ffcdc6f6dccd9d374f4418d5727de5d772db05a7a'),
    'docs/images/native-sd-reset-trace-lab-26-error.jpg':
        (2806252, '7d120b6626112e52fe6c341567f1f91f66e7d4c3793e9bf01eec28f1d95aac9e'),
    'docs/images/native-acpi-gic-profile-lab-28.jpg':
        (2467336, 'a17ed05925470c9b352a51859148954243f36ab632dc5aefe465a7cb1ee8f9f5'),
    'docs/images/native-acpi-install-shape-lab-30.jpg':
        (2814280, '4da0742efd00a4e751cf10dc643f8cf86adfc7f56ac24e05585c17a17b8ab53d'),
    'docs/images/native-cpu-psci-profile-lab-31.jpg':
        (2271229, '2f1bed937a9210bdc2aee625026f579fb14803c58940336e4990803b2530adee'),
    'docs/images/native-sd-acpi-boot-lab-32-read.jpg':
        (3050358, '445e5d2e8526d004802403ccdc6ef26a974b14ee82eb260ccb7b1d90a44ff1c2'),
    'docs/images/native-sd-acpi-boot-lab-32-bugcheck.jpg':
        (3121493, '3ba4ad72ac60beabcafc8ed60ab8022220d1d130706bcd42885c07153d6798dc'),
    'docs/images/native-winpe-crash-parameters-lab-33-read.jpg':
        (2986657, '5979860d9ad0a6d33238e74f32c89c702eb04212dd02d2e6bb2dbcbc2923d7b2'),
    'docs/images/native-winpe-crash-parameters-lab-33-bugcheck.jpg':
        (2320586, '5c48897c18c22102bcf7a1850c31463edc5c470bd4dcfc413097f0af186efa43'),
    'docs/images/native-winpe-crash-parameters-lab-33-bugcheck-detail.jpg':
        (2394661, 'c2b746ab95c70128508c13c366873d76de326971312a889dbc72680d82a531a3'),
    'docs/images/native-winpe-clean-reference-lab-35-logo.jpg':
        (2454604, '828b97ccfc20ef90489d6f2c83d8eb277027c248ba626e94334b375a90da5a29'),
    'docs/images/native-winpe-clean-reference-lab-35-bugcheck-detail.jpg':
        (3371935, 'e4c56b156857a00acee88edb6834282dfbc5c518695b7622d7d04e0f76f2284c'),
    'docs/images/native-display-profile-probe-36.jpg':
        (3005979, 'd5ba724631e1cc7a7449887ac6a40e5d4ddad8410fdf544d770777c30d40db4b'),
    'docs/images/native-display-clean-boot-lab-37-bugcheck.jpg':
        (1881566, '1666ca841d543b28f0e6234206aaac45e9f75a691a9e757f0e68eba4593d6f71'),
    'docs/images/native-winpe-reference-parameters-lab-38-bugcheck.jpg':
        (2024052, '7daa7823b2b537f7ebce9e73ec1d2fb06b17c92f9ee2b6e349f098c4651419e7'),
    'docs/images/native-winpe-reference-parameters-lab-38-bugcheck-detail.jpg':
        (1327598, '20aadc302fa4167e9089fa9d852953284d7d78fb6c3471a8a9df3790defce256'),
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
