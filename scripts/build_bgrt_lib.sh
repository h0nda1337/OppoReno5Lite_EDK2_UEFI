#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# Host validation and ARM64 object only; no display, ACPI install or phone access.
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to a reviewed EDK2 source directory}"
mkdir -p .build/bgrt
pkg="$root/OppoReno5LitePkg"
source="$pkg/Library/OppoBgrtLib/OppoBgrtLib.c"
common=(-std=c11 -Wall -Wextra -Werror -fshort-wchar -I"$edk2/MdePkg/Include" -I"$pkg/Include")
gcc "${common[@]}" -I"$edk2/MdePkg/Include/X64" \
  "$source" scripts/test_bgrt.c -o .build/bgrt/test-bgrt
.build/bgrt/test-bgrt "$@"
aarch64-linux-gnu-gcc "${common[@]}" -I"$edk2/MdePkg/Include/AArch64" \
  -ffreestanding -fno-stack-protector -fno-builtin -mstrict-align \
  -mgeneral-regs-only -c "$source" -o .build/bgrt/OppoBgrtLib.o
aarch64-linux-gnu-readelf -h .build/bgrt/OppoBgrtLib.o > .build/bgrt/elf-header.txt
printf '%s\n' 'BGRT library object compiled for ARM64; firmware execution untested.'
