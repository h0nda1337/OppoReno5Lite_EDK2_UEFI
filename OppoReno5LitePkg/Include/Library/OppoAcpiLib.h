/** @file
  Original, allocation-free ACPI serialization prototype. Not in native firmware.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_ACPI_LIB_H
#define OPPO_ACPI_LIB_H

#include <Uefi.h>

#define OPPO_ACPI_CPU_COUNT 8

typedef struct {
  UINT64 Mpidr;
  UINT64 RedistributorBase;
} OPPO_ACPI_CPU;

typedef struct {
  // Affinity fields only, with the other MPIDR_EL1 bits removed by the caller.
  UINT64 BootMpidr;
  // Order defines CPU _UIDs. Entry zero must describe the executing processor.
  OPPO_ACPI_CPU Cpu[OPPO_ACPI_CPU_COUNT];
} OPPO_ACPI_PROFILE;

typedef struct {
  UINT32 TotalBytes;
  UINT32 RsdpOffset;
  UINT32 XsdtOffset;
  UINT32 FadtOffset;
  UINT32 DsdtOffset;
  UINT32 MadtOffset;
  UINT32 GtdtOffset;
} OPPO_ACPI_LAYOUT;

UINTN EFIAPI OppoAcpiTableBytes (VOID);

/**
  Serialize RSDP/XSDT/FADT/CPU-only DSDT/MADT/GTDT using ACPI 6.0 layouts.

  Profile addresses MUST come from a separately reviewed runtime observation of
  each redistributor's affinity. This function validates ranges and uniqueness,
  not the truth of a hardware observation. It never infers a CPU's redistributor
  from its array index or its device-tree node name. See docs/ACPI.md for the
  fixed MT6779 FDT facts, assumptions and remaining platform requirements.

  Buffer is 8-byte aligned and at least OppoAcpiTableBytes() bytes long. Its
  eventual physical address is supplied separately; caller must allocate and
  preserve suitable ACPI reclaim memory before installing any configuration
  table. PhysicalBase is nonzero and 16-byte aligned. Profile, Layout and Buffer
  must not overlap. On any failure Buffer and Layout are unchanged.

  No allocation, MMIO, UEFI service, configuration-table installation or AML
  hardware access occurs. This is a serializer, not a complete Windows platform.
**/
EFI_STATUS EFIAPI OppoBuildAcpiTables (
  IN CONST OPPO_ACPI_PROFILE *Profile,
  OUT VOID *Buffer, IN UINTN Bytes, IN UINT64 PhysicalBase,
  OUT OPPO_ACPI_LAYOUT *Layout
  );

#endif
