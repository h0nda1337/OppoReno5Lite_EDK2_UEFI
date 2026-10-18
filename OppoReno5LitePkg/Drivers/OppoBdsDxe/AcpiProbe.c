/** @file
  Isolated read-only GIC observation; no SD, ACPI installation or Windows launch.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Uefi.h>
#include <Guid/OppoAcpiHandoff.h>
#include <OppoNativeProfile.h>
#include <Library/ArmLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/OppoAcpiProbeLib.h>

typedef struct { UINT32 Reads, LastValue; UINT64 LastAddress; } READ_TRACE;
STATIC EFI_STATUS EFIAPI ReadGic (VOID *Context, UINT64 Address, UINT32 *Value)
{
  READ_TRACE *T = Context;
  BOOLEAN Allowed = Address == OPPO_NATIVE_GICD_BASE || Address == OPPO_NATIVE_GICD_BASE + 4 ||
                    Address == OPPO_NATIVE_GICD_BASE + 0xffe8;
  if (Address >= OPPO_NATIVE_GICR_BASE && Address < OPPO_NATIVE_GICR_BASE + 0x200000ULL) {
    UINT64 Offset = (Address - OPPO_NATIVE_GICR_BASE) % 0x20000;
    Allowed = (BOOLEAN)(Offset == 8 || Offset == 12 || Offset == 0xffe8);
  }
  if (!Allowed) return EFI_ACCESS_DENIED;
  *Value = MmioRead32 ((UINTN)Address);
  T->Reads++; T->LastAddress = Address; T->LastValue = *Value;
  return EFI_SUCCESS;
}

VOID OppoAcpiProbeLab (VOID)
{
  STATIC CONST volatile CHAR8 Identity[] = "OPPO_ACPI_GIC_PROFILE_READ_ONLY_LAB_V1";
  OPPO_ACPI_GIC_OBSERVATION Observation;
  READ_TRACE Trace = {0,0,0};
  VOID *Hob;
  EFI_STATUS Status = EFI_COMPROMISED_DATA;
  if (!FeaturePcdGet (PcdNativeAcpiProfileLab) || Identity[0] != 'O') return;
  Hob = GetFirstGuidHob (&gOppoAcpiHandoffHobGuid);
  if (Hob != NULL && GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (OPPO_ACPI_HANDOFF)) {
    CONST OPPO_ACPI_HANDOFF *H = GET_GUID_HOB_DATA (Hob);
    if (H->Signature == OPPO_ACPI_HANDOFF_SIGNATURE && H->Revision == OPPO_ACPI_HANDOFF_REVISION &&
        H->Reserved == 0 && H->FdtBytes >= 40 && H->FdtBytes <= 2U * 1024 * 1024 &&
        (H->FdtBase & 7U) == 0 && H->FdtBase >= OPPO_NATIVE_DRAM_BASE &&
        H->FdtBase <= (UINT64)OPPO_NATIVE_DRAM_END - H->FdtBytes) {
      Status = OppoAcpiObserveGic ((CONST VOID *)(UINTN)H->FdtBase, H->FdtBytes,
                                  ArmReadMpidr (), ReadGic, &Trace, &Observation);
    }
  }
  DEBUG ((DEBUG_INFO, "ACPI PROFILE=%r READS=%u LAST=%Lx VALUE=%x\n",
          Status, Trace.Reads, Trace.LastAddress, Trace.LastValue));
  if (Status == EFI_SUCCESS) {
    DEBUG ((DEBUG_INFO, "ACPI BOOT AFF=%Lx GICD PIDR=%x TYPER=%x CTLR=%x\n",
            Observation.Profile.BootMpidr, Observation.DistributorPidr2,
            Observation.DistributorTyper, Observation.DistributorControl));
    for (UINTN I = 0; I < OPPO_ACPI_CPU_COUNT; I++)
      DEBUG ((DEBUG_INFO, "ACPI CPU%u AFF=%Lx GICR=%Lx TYPE=%Lx PIDR=%x\n", (UINT32)I,
              Observation.Profile.Cpu[I].Mpidr, Observation.Profile.Cpu[I].RedistributorBase,
              Observation.RedistributorTyper[I], Observation.RedistributorPidr2[I]));
  }
  DEBUG ((DEBUG_INFO, "ACPI READ-ONLY: NO TABLES INSTALLED; NO SD OR OS LAUNCH\n"));
}
