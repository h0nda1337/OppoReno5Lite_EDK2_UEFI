/** @file
  Original bounded, allocation-free FDT reader for platform handoff inspection.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_FDT_LIB_H
#define OPPO_FDT_LIB_H

#include <Uefi.h>
#include <Library/OppoPlatformLib.h>

typedef struct {
  CONST UINT8 *Data;
  UINT32 TotalBytes;
  UINT32 StructureOffset;
  UINT32 StructureBytes;
  UINT32 StringsOffset;
  UINT32 StringsBytes;
  UINT32 ReserveOffset;
  UINT32 ReserveCount;
} OPPO_FDT_VIEW;

/**
  Validate an FDT compatible with v17, at most 2 MiB, with bounded depth/names.
  No property value is interpreted and no physical memory is accessed beyond the
  caller-supplied buffer. Data must remain immutable while View is in use.
**/
BOOLEAN EFIAPI OppoFdtOpen (IN CONST VOID *Data, IN UINTN Bytes, OUT OPPO_FDT_VIEW *View);

/**
  Read a property by absolute path, rejecting duplicate selected properties or
  ambiguous duplicate selected nodes. Revalidates the token stream. Value points
  into the immutable input; no allocation is performed. Optional absence returns
  EFI_NOT_FOUND, while malformed/ambiguous input returns EFI_COMPROMISED_DATA.
**/
EFI_STATUS EFIAPI OppoFdtProperty (
  IN CONST OPPO_FDT_VIEW *View, IN CONST CHAR8 *Path, IN CONST CHAR8 *Name,
  OUT CONST VOID **Value, OUT UINT32 *Bytes
  );

BOOLEAN EFIAPI OppoFdtReservation (
  IN CONST OPPO_FDT_VIEW *View, IN UINT32 Index, OUT OPPO_MEMORY_RANGE *Range
  );

#endif
