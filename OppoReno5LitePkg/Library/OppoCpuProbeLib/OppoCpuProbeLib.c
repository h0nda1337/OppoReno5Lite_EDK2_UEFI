/** @file
  Original read-only PSCI/local-CPU observation policy. No allocation or MMIO.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Library/OppoCpuProbeLib.h>
#include <Library/BaseMemoryLib.h>
#include <IndustryStandard/ArmStdSmc.h>

STATIC BOOLEAN ProfileValid (CONST OPPO_ACPI_PROFILE *P)
{
  UINT32 Seen = 0;
  if (P->BootMpidr != P->Cpu[0].Mpidr || (P->BootMpidr & ~0x700ULL) != 0) return FALSE;
  for (UINTN I = 0; I < OPPO_ACPI_CPU_COUNT; I++) {
    UINT64 Aff = P->Cpu[I].Mpidr;
    if ((Aff & ~0x700ULL) != 0 || (Seen & (1U << (UINT32)(Aff >> 8))) != 0) return FALSE;
    Seen |= 1U << (UINT32)(Aff >> 8);
  }
  return (BOOLEAN)(Seen == 0xffU);
}

EFI_STATUS EFIAPI OppoObserveCpu (
  CONST OPPO_ACPI_PROFILE *Profile, OPPO_CPU_READ_REGISTER ReadRegister,
  OPPO_CPU_READ_PSCI ReadPsci, VOID *Context, OPPO_CPU_OBSERVATION *Observation
  )
{
  OPPO_CPU_OBSERVATION O;
  EFI_STATUS S;
  UINT64 Raw;
  if (Profile == NULL || Observation == NULL || ReadRegister == NULL || ReadPsci == NULL ||
      (UINTN)Profile > MAX_UINTN - sizeof (*Profile) ||
      (UINTN)Observation > MAX_UINTN - sizeof (*Observation) ||
      ((UINTN)Profile < (UINTN)Observation + sizeof (*Observation) &&
       (UINTN)Observation < (UINTN)Profile + sizeof (*Profile))) return EFI_INVALID_PARAMETER;
  if (!ProfileValid (Profile)) return EFI_UNSUPPORTED;
  ZeroMem (&O, sizeof (O));
#define REG(ID, FIELD) do { S = ReadRegister (Context, (ID), &O.FIELD); if (S != EFI_SUCCESS) return S; } while (0)
#define PSCI(F,A,B) do { S = ReadPsci (Context, (F), (A), (B), &Raw); if (S != EFI_SUCCESS) return S; } while (0)
  REG (OppoCpuEl, CurrentEl);
  if (O.CurrentEl != 4 && O.CurrentEl != 8) return EFI_UNSUPPORTED;
  REG (OppoCpuMpidr, Mpidr);
  if ((O.Mpidr & ~(0x000000ff00ffffffULL | BIT31 | BIT24)) != 0 ||
      (O.Mpidr & 0x000000ff00ffffffULL) != Profile->BootMpidr) return EFI_COMPROMISED_DATA;
  REG (OppoCpuMidr, Midr); REG (OppoCpuPfr0, Pfr0);
  if (O.Midr > MAX_UINT32 || ((O.Pfr0 >> 24) & 15U) != 1) return EFI_UNSUPPORTED;
  REG (OppoCpuSre, Sre);
  if (O.Sre > MAX_UINT32) return EFI_COMPROMISED_DATA;
  PSCI (ARM_SMC_ID_PSCI_VERSION, 0, 0);
  O.PsciVersion = (UINT32)Raw; // SMC32 only defines the low 32 return bits.
  if (O.PsciVersion != 2 && (O.PsciVersion >> 16) != 1) return EFI_UNSUPPORTED;
  if (O.PsciVersion >= 0x10000U) {
    O.FeaturesQueried = TRUE;
    PSCI (ARM_SMC_ID_PSCI_FEATURES, ARM_SMC_ID_PSCI_CPU_ON_AARCH64, 0);
    O.CpuOnFeature = (INT32)(UINT32)Raw;
    if (O.CpuOnFeature != 0 && O.CpuOnFeature != -1) return EFI_COMPROMISED_DATA;
    PSCI (ARM_SMC_ID_PSCI_FEATURES, ARM_SMC_ID_PSCI_AFFINITY_INFO_AARCH64, 0);
    O.AffinityFeature = (INT32)(UINT32)Raw;
    if (O.AffinityFeature != 0 && O.AffinityFeature != -1) return EFI_COMPROMISED_DATA;
  }
  for (UINTN I = 0; I < OPPO_ACPI_CPU_COUNT; I++) {
    PSCI (ARM_SMC_ID_PSCI_AFFINITY_INFO_AARCH64, Profile->Cpu[I].Mpidr, 0);
    O.AffinityRaw[I] = Raw;
    if (Raw <= 2) O.AffinityState[I] = (INT64)Raw;
    else {
      INT32 Low = (INT32)(UINT32)Raw;
      // Record old implementations' zero-extended 32-bit errors separately.
      if (Low < -9 || Low > -1 || (Raw != (UINT64)(INT64)Low && Raw != (UINT32)Low))
        return EFI_COMPROMISED_DATA;
      O.AffinityState[I] = Low;
      if (Raw == (UINT32)Low) O.Legacy32BitErrorMask |= (UINT8)(1U << I);
    }
  }
  O.BootReportedOn = (BOOLEAN)(O.AffinityState[0] == ARM_SMC_ID_PSCI_AFFINITY_INFO_ON);
  CopyMem (Observation, &O, sizeof (O));
  return EFI_SUCCESS;
#undef REG
#undef PSCI
}
