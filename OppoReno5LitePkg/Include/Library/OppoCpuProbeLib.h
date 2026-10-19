/** @file
  Original bounded observation of local CPU registers and read-only PSCI calls.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_CPU_PROBE_LIB_H
#define OPPO_CPU_PROBE_LIB_H
#include <Library/OppoAcpiLib.h>

typedef enum { OppoCpuEl, OppoCpuMpidr, OppoCpuMidr, OppoCpuPfr0, OppoCpuSre } OPPO_CPU_REGISTER;
typedef EFI_STATUS (EFIAPI *OPPO_CPU_READ_REGISTER)(VOID *Context, OPPO_CPU_REGISTER Register, UINT64 *Value);
typedef EFI_STATUS (EFIAPI *OPPO_CPU_READ_PSCI)(VOID *Context, UINT32 Function, UINT64 Arg1, UINT64 Arg2, UINT64 *Value);

typedef struct {
  UINT64 CurrentEl, Mpidr, Midr, Pfr0, Sre;
  UINT32 PsciVersion;
  BOOLEAN FeaturesQueried;
  BOOLEAN BootReportedOn;
  UINT8 Legacy32BitErrorMask;
  UINT8 Reserved;
  INT32 CpuOnFeature, AffinityFeature;
  UINT64 AffinityRaw[OPPO_ACPI_CPU_COUNT];
  INT64 AffinityState[OPPO_ACPI_CPU_COUNT];
} OPPO_CPU_OBSERVATION;

/**
  Caller first validates this boot's FDT and GIC profile. Check affinity set and
  executing CPU before callback queries. Read local EL/MPIDR/MIDR/PFR0/current-EL
  SRE; gate the SRE read on the GIC system-register feature. Query PSCI_VERSION,
  two PSCI_FEATURES targets on v1.x, and eight AFFINITY_INFO64 calls at level0.
  Never call CPU_ON/OFF/SUSPEND or reset. Never write a system register.
  Output is unchanged on error, and must not overlap the stable input Profile.
  A successful observation does not prove secondary execution, interrupt delivery,
  GICv2 compatibility or OS readiness. See docs/ACPI.md for return-code semantics.
**/
EFI_STATUS EFIAPI OppoObserveCpu (
  CONST OPPO_ACPI_PROFILE *Profile, OPPO_CPU_READ_REGISTER ReadRegister,
  OPPO_CPU_READ_PSCI ReadPsci, VOID *Context, OPPO_CPU_OBSERVATION *Observation
  );
#endif
