#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
python3 scripts/acpi_probe_fixtures.py
sources=(OppoReno5LitePkg/Library/OppoAcpiProbeLib/OppoAcpiProbeLib.c
  OppoReno5LitePkg/Library/OppoFdtLib/OppoFdtLib.c
  OppoReno5LitePkg/Library/OppoAcpiLib/OppoAcpiLib.c)
common=(-std=c11 -Wall -Wextra -Werror -fshort-wchar
  -I"$edk2/MdePkg/Include" -IOppoReno5LitePkg/Include)
gcc "${common[@]}" -I"$edk2/MdePkg/Include/X64" -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "${sources[@]}" scripts/test_acpi_probe.c -o .build/acpi-probe/test-probe
.build/acpi-probe/test-probe .build/acpi-probe/good.dtb .build/acpi-probe/bad-*.dtb
if [[ "$#" == 1 ]]; then .build/acpi-probe/test-probe "$1"; fi
aarch64-linux-gnu-gcc "${common[@]}" -I"$edk2/MdePkg/Include/AArch64" \
  -O2 -ffreestanding -fno-stack-protector -fno-builtin -mstrict-align \
  -mgeneral-regs-only -c "${sources[0]}" -o .build/acpi-probe/OppoAcpiProbeLib.o
printf '%s\n' 'Original GIC observation compiled for ARM64; host register samples do not prove physical CPU/GIC mapping.'
