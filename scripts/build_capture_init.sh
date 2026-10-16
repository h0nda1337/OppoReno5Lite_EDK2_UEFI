#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
mkdir -p .build/diagnostic
aarch64-linux-gnu-gcc -std=c11 -Wall -Wextra -Werror -O2 -static \
  scripts/capture_init.c -o .build/diagnostic/capture-init
aarch64-linux-gnu-readelf -h .build/diagnostic/capture-init > .build/diagnostic/init-elf.txt
if aarch64-linux-gnu-readelf -l .build/diagnostic/capture-init | grep -q INTERP; then
  printf '%s\n' 'Unexpected dynamic linker dependency' >&2
  exit 1
fi
set +e
qemu-aarch64 .build/diagnostic/capture-init > .build/diagnostic/pid-guard.txt 2>&1
status=$?
set -e
if [[ "$status" != 2 ]]; then
  printf '%s\n' 'Non-PID-1 execution guard failed' >&2
  exit 1
fi
cat .build/diagnostic/pid-guard.txt
printf '%s\n' 'Linux diagnostic init built; QEMU user-mode PID guard passed. No device operation.'
