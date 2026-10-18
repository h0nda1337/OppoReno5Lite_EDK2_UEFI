/** @file
  Original protected FDT reference for the isolated, read-only ACPI profile lab.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_ACPI_HANDOFF_H
#define OPPO_ACPI_HANDOFF_H
#include <Uefi.h>
#define OPPO_ACPI_HANDOFF_SIGNATURE 0x5043414fU
#define OPPO_ACPI_HANDOFF_REVISION 1U
typedef struct {
  UINT32 Signature;
  UINT32 Revision;
  UINT64 FdtBase;
  UINT32 FdtBytes;
  UINT32 Reserved;
} OPPO_ACPI_HANDOFF;
extern EFI_GUID gOppoAcpiHandoffHobGuid;
#endif
