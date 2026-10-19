/** @file
  Original boot-time ACPI publication transaction. No hardware access.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_ACPI_INSTALL_LIB_H
#define OPPO_ACPI_INSTALL_LIB_H
#include <Library/OppoAcpiLib.h>

typedef struct {
  EFI_PHYSICAL_ADDRESS Base;
  UINTN Pages;
  OPPO_ACPI_LAYOUT Layout;
  EFI_STATUS AllocateStatus, BuildStatus, MemoryStatus, PublishStatus, ReadbackStatus, CleanupStatus;
  BOOLEAN Published;
} OPPO_ACPI_INSTALL_REPORT;

/**
  At TPL_APPLICATION, publish one complete, original ACPI 2.0+ RSDP in gST.
  Caller supplies a profile validated against this boot's FDT and GIC samples.
  Reject existing ACPI 1.0/2.0 entries; never replace another provider's tables.
  Allocate page-aligned EfiACPIReclaimMemory, serialize all tables with physical
  pointers, and verify memory-map coverage before InstallConfigurationTable.
  After publication, verify the configuration entry, pointers and checksums.
  A published allocation remains owned by ACPI even if readback reports damage.
  On failure, release unpublished pages and report any cleanup failure.
  This firmware uses identity addressing before ExitBootServices. Profile and
  Report must be disjoint, stable caller storage outside any allocation made by
  this function. Report contains per-stage diagnostics even on failure.
  No MMIO, CPU_ON, ACPI protocol installation, OS launch or runtime operation.
**/
EFI_STATUS EFIAPI OppoInstallAcpi (
  IN CONST OPPO_ACPI_PROFILE *Profile, OUT OPPO_ACPI_INSTALL_REPORT *Report
  );
#endif
