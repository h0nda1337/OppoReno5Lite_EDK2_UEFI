#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# System emulation of the original SEC entry only, not an OPPO or UEFI boot.
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/sec-entry
cat > .build/sec-entry/test.ld <<'EOF'
ENTRY(TestImageStart)
SECTIONS {
  . = 0x40080000;
  .text : { KEEP(*(.test_header)) *(.text.OppoSecEntry) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss (NOLOAD) : { *(.bss*) *(COMMON) }
  . = ALIGN(4096);
  TestImageEnd = .;
  /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
EOF
common=(-Wall -Wextra -Werror -O2 -ffreestanding -fno-builtin -fno-stack-protector -fshort-wchar
  -fno-pic -fno-pie -ffixed-x18 -mgeneral-regs-only -mstrict-align
  -I"$edk2/MdePkg/Include" -I"$edk2/MdePkg/Include/AArch64" -IOppoReno5LitePkg/Include)
aarch64-linux-gnu-gcc "${common[@]}" -c OppoReno5LitePkg/Sec/AArch64/Entry.S -o .build/sec-entry/entry.o
aarch64-linux-gnu-gcc "${common[@]}" -c scripts/test_sec_header.S -o .build/sec-entry/header.o
for el in 1 2; do
  aarch64-linux-gnu-gcc "${common[@]}" -DTEST_EXPECTED_EL="$el" \
    -c scripts/test_sec_entry.c -o ".build/sec-entry/harness-el$el.o"
  aarch64-linux-gnu-ld -T .build/sec-entry/test.ld .build/sec-entry/header.o \
    .build/sec-entry/entry.o ".build/sec-entry/harness-el$el.o" -o ".build/sec-entry/test-el$el.elf"
  aarch64-linux-gnu-objcopy -O binary ".build/sec-entry/test-el$el.elf" ".build/sec-entry/test-el$el.img"
  virtualization=off
  if [[ "$el" == 2 ]]; then virtualization=on; fi
  timeout 15 qemu-system-aarch64 -machine "virt,virtualization=$virtualization" \
    -cpu cortex-a57 -m 512M -display none -serial none -monitor none -no-reboot \
    -semihosting-config enable=on,target=native -kernel ".build/sec-entry/test-el$el.img" \
    > ".build/sec-entry/qemu-el$el.txt" 2>&1
  grep -q '^PASS original SEC entry' ".build/sec-entry/qemu-el$el.txt"
  cat ".build/sec-entry/qemu-el$el.txt"
done
printf '%s\n' 'QEMU system EL1 and EL2 entry fixtures passed. No DXE/Shell or phone execution demonstrated.'
