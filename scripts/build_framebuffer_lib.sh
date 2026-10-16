#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/framebuffer
source=OppoReno5LitePkg/Library/OppoFrameBufferLib/OppoFrameBufferLib.c
common=(-std=c11 -Wall -Wextra -Werror -fshort-wchar
  -I"$edk2/MdePkg/Include" -IOppoReno5LitePkg/Include)
gcc "${common[@]}" -I"$edk2/MdePkg/Include/X64" -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$source" scripts/test_framebuffer.c -o .build/framebuffer/test-framebuffer
.build/framebuffer/test-framebuffer
aarch64-linux-gnu-gcc "${common[@]}" -I"$edk2/MdePkg/Include/AArch64" \
  -O2 -ffreestanding -fno-stack-protector -fno-builtin -mstrict-align \
  -mgeneral-regs-only -c "$source" -o .build/framebuffer/OppoFrameBufferLib.o
aarch64-linux-gnu-readelf -h .build/framebuffer/OppoFrameBufferLib.o > .build/framebuffer/elf-header.txt
printf '%s\n' 'Original framebuffer software library checked with ASan/UBSan and compiled for ARM64; no device operation.'
