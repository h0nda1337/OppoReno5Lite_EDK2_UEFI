#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# Build only. No recovery packaging, ADB, Fastboot, device writes or publication.
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
edk2="$(cd -- "${EDK2_SOURCE:?Set EDK2_SOURCE to reviewed EDK2 sources}" && pwd)"
mkdir -p "$root/.build/native"
log="$root/.build/native/build.log"
cpus="$(getconf _NPROCESSORS_ONLN)"
default_modules=$((cpus < 8 ? cpus : 8))
modules="${NATIVE_MODULE_JOBS:-$default_modules}"
if [[ ! "$modules" =~ ^[1-9][0-9]{0,3}$ ]] || ((modules > cpus)); then
  printf '%s\n' 'NATIVE_MODULE_JOBS must be a positive integer no greater than the available CPUs' >&2
  exit 1
fi
default_objects=$((cpus / modules))
((default_objects <= 4)) || default_objects=4
objects="${NATIVE_OBJECT_JOBS:-$default_objects}"
if [[ ! "$objects" =~ ^[1-9][0-9]{0,3}$ ]] || ((objects > cpus || modules * objects > cpus)); then
  printf '%s\n' 'NATIVE_MODULE_JOBS times NATIVE_OBJECT_JOBS must not exceed the available CPUs' >&2
  exit 1
fi
# EDK2 schedules independent modules; GNU make parallelizes their object files.
# Scope this budget to the build instead of changing global host configuration.
export MAKEFLAGS="-j$objects"
extra=()
case "${NATIVE_ENTRY_PROBE:-0}:${NATIVE_MMU_PROBE:-0}" in
  0:0) mode=integration ;;
  1:0) mode=entry-probe; extra=(-D OPPO_ENTRY_PROBE=TRUE) ;;
  0:1) mode=mmu-probe; extra=(-D OPPO_MMU_PROBE=TRUE) ;;
  *) printf '%s\n' 'Select at most one probe; NATIVE_ENTRY_PROBE and NATIVE_MMU_PROBE must be 0 or 1' >&2; exit 1 ;;
esac
export WORKSPACE="$edk2" PACKAGES_PATH="$root"
export GCC_AARCH64_PREFIX="${GCC_AARCH64_PREFIX:-aarch64-linux-gnu-}"
export SOURCE_DATE_EPOCH="$(git -C "$edk2" show -s --format=%ct HEAD)"
cd "$edk2"
set +u
source edksetup.sh BaseTools > /dev/null
set -u
printf 'Native build: %s CPUs, %s module jobs x %s object jobs (maximum %s).\n' \
  "$cpus" "$modules" "$objects" "$((modules * objects))"
if ! build -n "$modules" -a AARCH64 -t GCC -b RELEASE -p OppoReno5LitePkg/OppoNative.dsc "${extra[@]}" > "$log" 2>&1; then
  tail -n 24 "$log"
  exit 1
fi
if ! grep -q '^- Done -' "$log"; then
  tail -n 24 "$log"
  exit 1
fi
test -s "$edk2/.build/OppoNative/RELEASE_GCC/FV/OPPONATIVE.fd"
mkdir -p "$root/.build/native/$mode"
cp -- "$edk2/.build/OppoNative/RELEASE_GCC/FV/OPPONATIVE.fd" "$root/.build/native/$mode/OPPONATIVE.fd"
cp -- "$log" "$root/.build/native/$mode/build.log"
printf '%s\n' "Native $mode prototype built on PC. No phone execution is implied."
