#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/fdt
python3 - <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, 'scripts')
from test_platform_inventory import fixture
Path('.build/fdt/synthetic.dtb').write_bytes(fixture())
PY
source=OppoReno5LitePkg/Library/OppoFdtLib/OppoFdtLib.c
common=(-std=c11 -Wall -Wextra -Werror -fshort-wchar
  -I"$edk2/MdePkg/Include" -IOppoReno5LitePkg/Include)
gcc "${common[@]}" -I"$edk2/MdePkg/Include/X64" -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$source" scripts/test_fdt.c -o .build/fdt/test-fdt
args=(.build/fdt/synthetic.dtb)
if [[ "$#" == 1 ]]; then args+=("$1"); fi
.build/fdt/test-fdt "${args[@]}"
aarch64-linux-gnu-gcc "${common[@]}" -I"$edk2/MdePkg/Include/AArch64" \
  -O2 -ffreestanding -fno-stack-protector -fno-builtin -mstrict-align \
  -mgeneral-regs-only -c "$source" -o .build/fdt/OppoFdtLib.o
aarch64-linux-gnu-readelf -h .build/fdt/OppoFdtLib.o > .build/fdt/elf-header.txt
printf '%s\n' 'Original bounded FDT reader checked on host and compiled for ARM64; no phone operation.'
