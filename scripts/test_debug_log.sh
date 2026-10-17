#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/debug-log
for mode in sec core; do
extra=()
if [[ "$mode" == core ]]; then extra=(-DOPPO_LOG_EARLY_CORE); fi
gcc -std=c11 -Wall -Wextra -Werror -fshort-wchar -DNO_MSABI_VA_FUNCS -O1 -g "${extra[@]}" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$edk2/MdePkg/Include" -I"$edk2/MdePkg/Include/X64" -IOppoReno5LitePkg/Include \
  OppoReno5LitePkg/Library/OppoDebugLib/OppoDebugLib.c \
  OppoReno5LitePkg/Library/OppoFrameBufferLib/OppoFrameBufferLib.c \
  scripts/test_debug_log.c -o ".build/debug-log/test-debug-log-$mode"
".build/debug-log/test-debug-log-$mode"
done
