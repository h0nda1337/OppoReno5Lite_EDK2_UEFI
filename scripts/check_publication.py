#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Inspect staged Git blobs for accidental binary/private publication."""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ALLOWED_ROOT_FILES = {'README.md', 'LICENSE', 'AGENTS.md', '.gitignore', '.gitattributes'}
ALLOWED_DIRS = {'docs', 'scripts', 'evidence', 'OppoReno5LitePkg', '.github'}
ALLOWED_SUFFIXES = {'.md', '.py', '.json', '.c', '.h', '.s', '.asm', '.inf',
                    '.dsc', '.fdf', '.dec', '.inc', '.yml', '.yaml', '.txt', '.patch', '.sh'}
PRIVATE = [
    re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY-----'),
    re.compile(rb'gh[pousr]_[A-Za-z0-9]{30,}'),
    re.compile(rb'github_pat_[A-Za-z0-9_]{40,}'),
    re.compile(rb'(?im)^\s*(?:ro\.serialno|ro\.boot\.serialno|androidboot\.serialno|imei[12]?)\s*[=:]\s*\S+'),
]


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def validate(name, data):
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
    print('Publication check passed for', len(names), 'staged text files.')


if __name__ == '__main__':
    main()
