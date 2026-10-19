#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"; cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/acpi-install
gcc -std=c11 -Wall -Wextra -Werror -fshort-wchar -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$edk2/MdePkg/Include" -I"$edk2/MdePkg/Include/X64" -IOppoReno5LitePkg/Include \
  OppoReno5LitePkg/Library/OppoAcpiInstallLib/OppoAcpiInstallLib.c \
  OppoReno5LitePkg/Library/OppoAcpiLib/OppoAcpiLib.c scripts/test_acpi_install.c \
  -o .build/acpi-install/test-install
.build/acpi-install/test-install
