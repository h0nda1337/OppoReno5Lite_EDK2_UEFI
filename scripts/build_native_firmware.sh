#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# Build only. No recovery packaging, ADB, Fastboot, device writes or publication.
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
edk2="$(cd -- "${EDK2_SOURCE:?Set EDK2_SOURCE to reviewed EDK2 sources}" && pwd)"
mkdir -p "$root/.build/native"
log="$root/.build/native/build.log"
export WORKSPACE="$edk2" PACKAGES_PATH="$root"
export GCC_AARCH64_PREFIX="${GCC_AARCH64_PREFIX:-aarch64-linux-gnu-}"
export SOURCE_DATE_EPOCH="$(git -C "$edk2" show -s --format=%ct HEAD)"
cd "$edk2"
set +u
source edksetup.sh BaseTools > /dev/null
set -u
if ! build -n 4 -a AARCH64 -t GCC -b RELEASE -p OppoReno5LitePkg/OppoNative.dsc > "$log" 2>&1; then
  tail -n 24 "$log"
  exit 1
fi
if ! grep -q '^- Done -' "$log"; then
  tail -n 24 "$log"
  exit 1
fi
test -s "$edk2/.build/OppoNative/RELEASE_GCC/FV/OPPONATIVE.fd"
printf '%s\n' 'Native integration prototype built on PC. No phone execution is implied.'
