#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/gop
gcc -std=c11 -Wall -Wextra -Werror -fshort-wchar -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$edk2/MdePkg/Include" -I"$edk2/MdePkg/Include/X64" -IOppoReno5LitePkg/Include \
  OppoReno5LitePkg/Drivers/OppoGopDxe/OppoGopDxe.c \
  OppoReno5LitePkg/Library/OppoFrameBufferLib/OppoFrameBufferLib.c \
  scripts/test_gop.c -o .build/gop/test-gop
.build/gop/test-gop
