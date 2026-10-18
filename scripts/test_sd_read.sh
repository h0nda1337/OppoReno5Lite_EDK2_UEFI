#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/msdc
gcc -std=c11 -Wall -Wextra -Werror -fshort-wchar -DNO_MSABI_VA_FUNCS -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$edk2/MdePkg/Include" -I"$edk2/MdePkg/Include/X64" -IOppoReno5LitePkg/Include \
  scripts/test_sd_read.c -o .build/msdc/test-sd-read
.build/msdc/test-sd-read
