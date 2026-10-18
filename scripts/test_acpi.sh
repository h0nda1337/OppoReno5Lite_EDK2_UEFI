#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/acpi
source=OppoReno5LitePkg/Library/OppoAcpiLib/OppoAcpiLib.c
common=(-std=c11 -Wall -Wextra -Werror -fshort-wchar
  -I"$edk2/MdePkg/Include" -IOppoReno5LitePkg/Include)
gcc "${common[@]}" -I"$edk2/MdePkg/Include/X64" -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$source" scripts/test_acpi.c -o .build/acpi/test-acpi
.build/acpi/test-acpi
python3 scripts/test_acpi_bytes.py
iasl -ve -d .build/acpi/generated-dsdt.aml
iasl -ve -we -p .build/acpi/roundtrip .build/acpi/generated-dsdt.dsl
python3 - <<'PY'
from pathlib import Path
import re
s = Path('.build/acpi/generated-dsdt.dsl').read_text()
assert len(re.findall(r'Device \(CP0[0-7]\)', s)) == 8
assert len(re.findall(r'Name \(_HID, "ACPI0007"', s)) == 8
uids = re.findall(r'Name \(_UID, (Zero|One|0x[0-9A-Fa-f]+)\)', s)
assert len(uids) == 8
assert [0 if x == 'Zero' else 1 if x == 'One' else int(x, 16) for x in uids] == list(range(8))
assert not any(x in s for x in ('OperationRegion', 'Method (', 'Processor ('))
print('ACPICA disassembly/recompile: eight CPU devices and integer UIDs; no hardware methods')
PY
acpiexec -b 'evaluate \_SB.CP00._UID; evaluate \_SB.CP07._UID; evaluate \_SB.CP07._STA' \
  .build/acpi/generated-dsdt.aml > .build/acpi/acpiexec.txt 2>&1
python3 - <<'PY'
from pathlib import Path
import re
s = Path('.build/acpi/acpiexec.txt').read_text()
assert re.findall(r'\[Integer\] = ([0-9A-F]{16})', s) == ['0000000000000000', '0000000000000007', '000000000000000F']
assert '0 Regions,    0 Methods' in s and 'No outstanding allocations' in s
print('ACPICA interpreter: first/last CPU UIDs and CPU status evaluated correctly')
PY
aarch64-linux-gnu-gcc "${common[@]}" -I"$edk2/MdePkg/Include/AArch64" \
  -O2 -ffreestanding -fno-stack-protector -fno-builtin -mstrict-align \
  -mgeneral-regs-only -c "$source" -o .build/acpi/OppoAcpiLib.o
aarch64-linux-gnu-readelf -h .build/acpi/OppoAcpiLib.o > .build/acpi/elf-header.txt
printf '%s\n' 'ACPI serialization prototype checked on host and compiled for ARM64; not linked into native firmware or installed on phone.'
