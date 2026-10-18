/** @file
  Original conservative plan for the first UEFI memory pool and protections.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_MEMORY_PLAN_LIB_H
#define OPPO_MEMORY_PLAN_LIB_H

#include <Uefi.h>
#include <Library/OppoHandoffLib.h>

typedef struct {
  OPPO_MEMORY_RANGE Image;
  OPPO_MEMORY_RANGE Pool;
  OPPO_MEMORY_RANGE Protected[OPPO_MAX_BOOT_RESERVATIONS + 1];
  EFI_MEMORY_TYPE ProtectedType[OPPO_MAX_BOOT_RESERVATIONS + 1];
  UINTN ProtectedCount;
  UINT32 DynamicReservationsDeferredToOs;
  UINT32 EmptyNodesIgnored;
} OPPO_EARLY_MEMORY_PLAN;

typedef struct {
  OPPO_MEMORY_RANGE Free[OPPO_MAX_BOOT_RESERVATIONS + 2];
  UINTN Count;
  UINT64 TotalBytes;
} OPPO_ADDITIONAL_MEMORY_PLAN;

/**
  Plan a bounded pool, firmware image and page-rounded fixed exclusions. No HOBs,
  allocations, page-table changes or hardware accesses are made. Only the image
  and chosen pool are candidates for initial UEFI-owned RAM; this is not a map of
  all DRAM. Dynamic DT reservations remain deferred to the OS per DTSpec 3.5.4.
  Nonempty unresolved nodes and overlapping rounded exclusions are rejected.
  Failure leaves Plan unchanged.
**/
EFI_STATUS EFIAPI OppoPlanEarlyMemory (
  IN CONST OPPO_BOOT_HANDOFF *Handoff, IN OPPO_MEMORY_RANGE Image,
  IN OPPO_MEMORY_RANGE Pool, OUT OPPO_EARLY_MEMORY_PLAN *Plan
  );

/** Plan additional RAM within one bounded, page-aligned window (16 MiB..2 GiB).
    Revalidates the early plan, requires declared RAM, excludes every rounded
    fixed reservation, image and initial pool, and rejects ambiguous RAM banks.
    Dynamic OS reservations are not allocated here. No physical access occurs;
    failure leaves Additional unchanged. */
EFI_STATUS EFIAPI OppoPlanAdditionalMemory (
  IN CONST OPPO_BOOT_HANDOFF *Handoff,
  IN OPPO_MEMORY_RANGE Image, IN OPPO_MEMORY_RANGE Pool,
  IN OPPO_MEMORY_RANGE Window, OUT OPPO_ADDITIONAL_MEMORY_PLAN *Additional
  );

#endif
