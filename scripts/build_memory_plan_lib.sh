#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/memory-plan
source=OppoReno5LitePkg/Library/OppoMemoryPlanLib/OppoMemoryPlanLib.c
common=(-std=c11 -Wall -Wextra -Werror -fshort-wchar
  -I"$edk2/MdePkg/Include" -IOppoReno5LitePkg/Include)
gcc "${common[@]}" -I"$edk2/MdePkg/Include/X64" -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer "$source" \
  OppoReno5LitePkg/Library/OppoHandoffLib/OppoHandoffLib.c \
  OppoReno5LitePkg/Library/OppoFdtLib/OppoFdtLib.c \
  OppoReno5LitePkg/Library/OppoPlatformLib/OppoPlatformLib.c \
  scripts/test_memory_plan.c -o .build/memory-plan/test-memory-plan
.build/memory-plan/test-memory-plan "$@"
aarch64-linux-gnu-gcc "${common[@]}" -I"$edk2/MdePkg/Include/AArch64" \
  -O2 -ffreestanding -fno-stack-protector -fno-builtin -mstrict-align \
  -mgeneral-regs-only -c "$source" -o .build/memory-plan/OppoMemoryPlanLib.o
printf '%s\n' 'Original early memory planner compiled for ARM64; no HOB, page table or phone operation.'
