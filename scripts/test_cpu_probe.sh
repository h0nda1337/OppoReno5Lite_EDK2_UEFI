#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"; cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/cpu-probe
gcc -std=c11 -Wall -Wextra -Werror -fshort-wchar -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$edk2/MdePkg/Include" -I"$edk2/MdePkg/Include/X64" -IOppoReno5LitePkg/Include \
  OppoReno5LitePkg/Library/OppoCpuProbeLib/OppoCpuProbeLib.c scripts/test_cpu_probe.c \
  -o .build/cpu-probe/test-probe
.build/cpu-probe/test-probe
aarch64-linux-gnu-gcc -c -x assembler-with-cpp \
  OppoReno5LitePkg/Drivers/OppoBdsDxe/AArch64/CpuRegisters.S -o .build/cpu-probe/reads.o
aarch64-linux-gnu-objdump -d .build/cpu-probe/reads.o > .build/cpu-probe/reads-disassembly.txt
python3 - <<'PY'
from pathlib import Path
import re
s=Path('.build/cpu-probe/reads-disassembly.txt').read_text()
ops=re.findall(r'^\s*[0-9a-f]+:\s+[0-9a-f]+\s+(\w+)',s,re.M)
assert ops==['mrs','ret']*6,ops
print('Original ARM64 register helpers contain exactly six MRS/RET pairs; no write or SMC instructions')
PY
