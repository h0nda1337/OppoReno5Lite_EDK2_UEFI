/** @file
  Original bounded native binding for read-only local CPU and PSCI observations.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Uefi.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/OppoCpuProbeLib.h>
#include <IndustryStandard/ArmStdSmc.h>

UINT64 EFIAPI OppoCpuCurrentEl (VOID);
UINT64 EFIAPI OppoCpuMpidrValue (VOID);
UINT64 EFIAPI OppoCpuMidrValue (VOID);
UINT64 EFIAPI OppoCpuPfr0Value (VOID);
UINT64 EFIAPI OppoCpuSreEl1 (VOID);
UINT64 EFIAPI OppoCpuSreEl2 (VOID);
typedef struct { UINT32 Registers, Smcs, LastFunction, LastRegister; UINT64 LastArg1, LastResult, LastRegisterValue; } CPU_TRACE;

STATIC EFI_STATUS EFIAPI ReadRegister (VOID *Context, OPPO_CPU_REGISTER Register, UINT64 *Value)
{
  CPU_TRACE *T = Context;
  if (Value == NULL) return EFI_INVALID_PARAMETER;
  switch (Register) {
    case OppoCpuEl: *Value = OppoCpuCurrentEl (); break;
    case OppoCpuMpidr: *Value = OppoCpuMpidrValue (); break;
    case OppoCpuMidr: *Value = OppoCpuMidrValue (); break;
    case OppoCpuPfr0: *Value = OppoCpuPfr0Value (); break;
    case OppoCpuSre: {
      UINT64 El = OppoCpuCurrentEl ();
      if (El != 4 && El != 8) return EFI_UNSUPPORTED;
      *Value = El == 4 ? OppoCpuSreEl1 () : OppoCpuSreEl2 (); break;
    }
    default: return EFI_ACCESS_DENIED;
  }
  T->Registers++; T->LastRegister = Register; T->LastRegisterValue = *Value;
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI ReadPsci (VOID *Context, UINT32 Function, UINT64 Arg1, UINT64 Arg2, UINT64 *Value)
{
  BOOLEAN Allowed = Arg2 == 0 &&
    ((Function == ARM_SMC_ID_PSCI_VERSION && Arg1 == 0) ||
     (Function == ARM_SMC_ID_PSCI_FEATURES &&
      (Arg1 == ARM_SMC_ID_PSCI_CPU_ON_AARCH64 || Arg1 == ARM_SMC_ID_PSCI_AFFINITY_INFO_AARCH64)) ||
     (Function == ARM_SMC_ID_PSCI_AFFINITY_INFO_AARCH64 && (Arg1 & ~0x700ULL) == 0));
  if (!Allowed) return EFI_ACCESS_DENIED;
  if (Value == NULL) return EFI_INVALID_PARAMETER;
  ARM_SMC_ARGS Args;
  ZeroMem (&Args, sizeof (Args)); Args.Arg0 = Function; Args.Arg1 = Arg1; Args.Arg2 = Arg2;
  ArmCallSmc (&Args);
  CPU_TRACE *T = Context;
  T->Smcs++; T->LastFunction = Function; T->LastArg1 = Arg1; T->LastResult = Args.Arg0;
  *Value = Args.Arg0;
  return EFI_SUCCESS;
}

VOID OppoCpuProbeLab (CONST OPPO_ACPI_PROFILE *Profile)
{
  STATIC CONST volatile CHAR8 Identity[] = "OPPO_CPU_PSCI_READ_ONLY_NO_OS_LAB_V1";
  CPU_TRACE Trace = {0};
  OPPO_CPU_OBSERVATION O;
  if (!FeaturePcdGet (PcdNativeCpuProfileLab) || Identity[0] != 'O') return;
  EFI_STATUS S = OppoObserveCpu (Profile, ReadRegister, ReadPsci, &Trace, &O);
  if (S == EFI_SUCCESS) {
    for (UINTN I = 0; I < OPPO_ACPI_CPU_COUNT; I++)
      DEBUG ((DEBUG_INFO, "PSCI CPU%u AFF=%Lx STATE=%Ld RAW=%Lx\n", (UINT32)I,
        Profile->Cpu[I].Mpidr, O.AffinityState[I], O.AffinityRaw[I]));
    DEBUG ((DEBUG_INFO, "CPU EL=%Lx MPIDR=%Lx MIDR=%Lx PFR0=%Lx\n", O.CurrentEl, O.Mpidr, O.Midr, O.Pfr0));
    DEBUG ((DEBUG_INFO, "CPU SRE=%Lx BOOT_ON=%u LEGACY_ERRORS=%x\n", O.Sre, (UINT32)O.BootReportedOn, O.Legacy32BitErrorMask));
    DEBUG ((DEBUG_INFO, "PSCI VERSION=%x FEATURES=%u CPU_ON64=%d AFF64=%d\n",
      O.PsciVersion, (UINT32)O.FeaturesQueried, O.CpuOnFeature, O.AffinityFeature));
  }
  DEBUG ((DEBUG_INFO, "CPU PROFILE=%r REG_READS=%u SMCS=%u LAST=%x ARG=%Lx RESULT=%Lx\n",
    S, Trace.Registers, Trace.Smcs, Trace.LastFunction, Trace.LastArg1, Trace.LastResult));
  if (S != EFI_SUCCESS) DEBUG ((DEBUG_INFO, "CPU LAST_REG=%u VALUE=%Lx\n", Trace.LastRegister, Trace.LastRegisterValue));
  DEBUG ((DEBUG_INFO, "CPU READ-ONLY: NO CPU START; NO TABLES, SD OR OS LAUNCH\n"));
}
