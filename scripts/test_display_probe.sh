#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/display-probe
python3 scripts/display_probe_fixture.py .build/display-probe/synthetic.dtb
gcc -std=c11 -Wall -Wextra -Werror -fshort-wchar -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$edk2/MdePkg/Include" -I"$edk2/MdePkg/Include/X64" \
  -IOppoReno5LitePkg/Include -IOppoReno5LitePkg/Sec \
  OppoReno5LitePkg/Sec/DisplayProbe.c \
  OppoReno5LitePkg/Library/OppoFdtLib/OppoFdtLib.c \
  scripts/test_display_probe.c -o .build/display-probe/test-display-probe
.build/display-probe/test-display-probe .build/display-probe/synthetic.dtb "$@"
