#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# Compile only the original libraries with EDK2's build system. No FV or phone.
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
edk2="$(cd -- "${EDK2_SOURCE:?Set EDK2_SOURCE to reviewed EDK2 sources}" && pwd)"
mkdir -p "$root/.build/edk2-libraries"
log="$root/.build/edk2-libraries/build.log"
export WORKSPACE="$edk2"
export PACKAGES_PATH="$root"
export GCC_AARCH64_PREFIX="${GCC_AARCH64_PREFIX:-aarch64-linux-gnu-}"
export SOURCE_DATE_EPOCH="$(git -C "$edk2" show -s --format=%ct HEAD)"
cd "$edk2"
# The upstream environment setup reads optional, possibly unset variables.
set +u
source edksetup.sh BaseTools > /dev/null
set -u
if ! build -a AARCH64 -t GCC -b RELEASE -p OppoReno5LitePkg/OppoLibraryValidation.dsc > "$log" 2>&1; then
  tail -n 20 "$log"
  exit 1
fi
if ! grep -q '^- Done -' "$log"; then
  tail -n 20 "$log"
  exit 1
fi
for name in OppoPlatformLib OppoBgrtLib OppoFrameBufferLib OppoFdtLib OppoHandoffLib OppoMemoryPlanLib; do
  archive="$edk2/.build/OppoLibraryValidation/RELEASE_GCC/AARCH64/OppoReno5LitePkg/Library/$name/$name/OUTPUT/$name.lib"
  test -s "$archive"
  aarch64-linux-gnu-ar t "$archive" | grep -q "$name.obj"
done
printf '%s\n' 'Six original libraries built with EDK2/AARCH64/GCC RELEASE. No firmware volume generated.'
