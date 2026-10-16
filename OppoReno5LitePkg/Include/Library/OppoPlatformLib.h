/** @file
  Static SoC description and conservative load-region validation.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_PLATFORM_LIB_H
#define OPPO_PLATFORM_LIB_H

#include <Base.h>

typedef struct {
  UINT64 Base;
  UINT64 Length;
} OPPO_MEMORY_RANGE;

typedef struct {
  UINT32 CpuCount;
  UINT64 GicDistributor;
  UINT64 GicRedistributors;
  UINT64 Watchdog;
  UINT64 Uart0;
  UINT64 Uart1;
} OPPO_SOC_DESCRIPTION;

CONST OPPO_SOC_DESCRIPTION *
EFIAPI
OppoGetSocDescription (VOID);

/**
  Validate a proposed physical load region against a verified runtime map.

  This function neither allocates nor accesses physical memory. A missing,
  malformed or unverified runtime map always rejects the proposed region.
**/
BOOLEAN
EFIAPI
OppoLoadRegionIsUsable (
  IN BOOLEAN                  RuntimeMapVerified,
  IN UINT64                   Base,
  IN UINT64                   Length,
  IN CONST OPPO_MEMORY_RANGE  *Banks,
  IN UINTN                    BankCount,
  IN CONST OPPO_MEMORY_RANGE  *Reserved,
  IN UINTN                    ReservedCount
  );

#endif
