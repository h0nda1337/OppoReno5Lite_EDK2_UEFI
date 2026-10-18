/** @file
  Original bounded FDT gate and read-only GIC identity probe for ACPI preparation.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_ACPI_PROBE_LIB_H
#define OPPO_ACPI_PROBE_LIB_H
#include <Library/OppoAcpiLib.h>

typedef EFI_STATUS (EFIAPI *OPPO_GIC_READ32)(
  IN VOID *Context, IN UINT64 Address, OUT UINT32 *Value
  );

typedef struct {
  OPPO_ACPI_PROFILE Profile;
  UINT32 DistributorPidr2;
  UINT32 DistributorTyper;
  UINT32 DistributorControl;
  UINT32 RedistributorPidr2[OPPO_ACPI_CPU_COUNT];
  UINT64 RedistributorTyper[OPPO_ACPI_CPU_COUNT];
} OPPO_ACPI_GIC_OBSERVATION;

/**
  Validate this boot's FDT CPU/GIC/timer/PMU/PSCI contracts BEFORE any callback.
  Read only GICD CTLR/TYPER/PIDR2 and GICR PIDR2/TYPER in the declared region.
  Reject unsupported architectures, VLPIs, unstable types, missing Last, duplicate
  or unknown affinities. Never write registers or infer CPU affinity from order.
  No allocations or UEFI services. Failed output stays unchanged. Fdt must remain
  immutable, and cannot overlap Observation. RawBootMpidr is MPIDR_EL1 including
  its permitted U/MT bits; Profile contains only affinity fields, boot CPU first.

  This observation does not validate CPU_ON, GICv2 compatibility, power domains,
  or a complete ACPI/Windows platform. It does not install any ACPI table.
**/
EFI_STATUS EFIAPI OppoAcpiObserveGic (
  IN CONST VOID *Fdt, IN UINTN Bytes, IN UINT64 RawBootMpidr,
  IN OPPO_GIC_READ32 Read, IN VOID *Context,
  OUT OPPO_ACPI_GIC_OBSERVATION *Observation
  );
#endif
