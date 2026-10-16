#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# Host checks and an ARM64 object build; no firmware image or device operations.
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to a reviewed EDK2 source directory}"
mkdir -p .build/platform
pkg="$root/OppoReno5LitePkg"
source="$pkg/Library/OppoPlatformLib/OppoPlatformLib.c"
common=(-std=c11 -Wall -Wextra -Werror -fshort-wchar -I"$edk2/MdePkg/Include" -I"$pkg/Include")
gcc "${common[@]}" -I"$edk2/MdePkg/Include/X64" \
  "$source" scripts/test_platform.c -o .build/platform/test-platform
.build/platform/test-platform
aarch64-linux-gnu-gcc "${common[@]}" -I"$edk2/MdePkg/Include/AArch64" \
  -ffreestanding -fno-stack-protector -fno-builtin -mstrict-align \
  -mgeneral-regs-only -c "$source" -o .build/platform/OppoPlatformLib.o
aarch64-linux-gnu-readelf -h .build/platform/OppoPlatformLib.o > .build/platform/elf-header.txt
python3 -m unittest discover -s scripts -p 'test_*.py' -v
printf '%s\n' 'Host region checks passed; ARM64 platform-library object compiled.'
