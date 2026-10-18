/** @file
  Original read-only interpretation of the MT6779 boot handoff.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_HANDOFF_LIB_H
#define OPPO_HANDOFF_LIB_H

#include <Uefi.h>
#include <Library/OppoPlatformLib.h>
#include <Library/OppoFdtLib.h>

#define OPPO_MAX_RAM_BANKS 8
#define OPPO_MAX_BOOT_RESERVATIONS 64
#define OPPO_RESERVATION_NO_MAP 1U
#define OPPO_RESERVATION_MEMRESERVE 2U
#define OPPO_RESERVATION_BOOT_DATA 4U

typedef struct {
  OPPO_MEMORY_RANGE Ram[OPPO_MAX_RAM_BANKS];
  OPPO_MEMORY_RANGE Reserved[OPPO_MAX_BOOT_RESERVATIONS];
  UINT32 ReservedFlags[OPPO_MAX_BOOT_RESERVATIONS];
  UINTN RamCount;
  UINTN ReservedCount;
  UINT32 DynamicReservationCount;
  UINT32 UnresolvedReservationCount;
  UINT32 EmptyReservationNodeCount;
  BOOLEAN FdtPhysicalAddressRecorded;
  BOOLEAN PsciSmcDeclared;
  BOOLEAN NativeFramebufferPresent;
  BOOLEAN NativePanelFound;
  BOOLEAN NativeFramebufferCoveredByReservation;
  BOOLEAN SplitFramebufferPresent;
  BOOLEAN FramebufferDeclarationsDisagree;
  OPPO_MEMORY_RANGE NativeFramebuffer;
  OPPO_MEMORY_RANGE SplitFramebuffer;
} OPPO_BOOT_HANDOFF;

/**
  Interpret MT6779 RAM, fixed/dynamic reservations, initrd, framebuffer tags and
  declared PSCI interface without accessing hardware or allocating any memory.

  PhysicalFdtBase zero denotes offline inspection of a file. A nonzero value
  is checked against declared RAM and protects that range from later allocation.
  Dynamic and unresolved reservations are counted, not silently allocated or
  discarded. The result is not a complete UEFI memory map or proof of a live
  display/PSCI call. Failure leaves Handoff unchanged.
**/
EFI_STATUS EFIAPI OppoInspectHandoff (
  IN CONST VOID *Fdt, IN UINTN Bytes, IN UINT64 PhysicalFdtBase,
  OUT OPPO_BOOT_HANDOFF *Handoff
  );

/** Require the MT6779 MSDC1 profile, including the named IOCFG_RM role and alias.
    Validating an existing register range alone does not establish its purpose.
    Pure inspection: no MMIO or allocations. */
BOOLEAN EFIAPI OppoSdProfileMatches (IN CONST OPPO_FDT_VIEW *View);
BOOLEAN EFIAPI OppoSdPowerProfileMatches (IN CONST OPPO_FDT_VIEW *View);
BOOLEAN EFIAPI OppoSdPowerEnableProfileMatches (IN CONST OPPO_FDT_VIEW *View);

#endif
