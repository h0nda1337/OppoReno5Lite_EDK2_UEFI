/** @file
  Original opt-in transaction gating the SD OS attempt on current-boot ACPI.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/OppoAcpiProbeLib.h>
#include <Library/OppoAcpiInstallLib.h>

EFI_STATUS OppoReadNativeAcpiProfile (OPPO_ACPI_GIC_OBSERVATION *Observation);
EFI_STATUS OppoCpuCheckForOs (CONST OPPO_ACPI_PROFILE *Profile);

EFI_STATUS OppoAcpiPrepareForOs (VOID)
{
  STATIC CONST volatile CHAR8 Identity[] = "OPPO_SD_ACPI_BOOT_LAB_V1";
  OPPO_ACPI_GIC_OBSERVATION Gic;
  OPPO_ACPI_INSTALL_REPORT R = {0};
  if (!FeaturePcdGet (PcdNativeSdAcpiBootLab) || Identity[0] != 'O') return EFI_ACCESS_DENIED;
  EFI_STATUS S = OppoReadNativeAcpiProfile (&Gic);
  if (S == EFI_SUCCESS) S = OppoCpuCheckForOs (&Gic.Profile);
  DEBUG ((DEBUG_INFO, "OS ACPI PROFILE AND CPU GATE=%r\n", S));
  if (S != EFI_SUCCESS) return S;
  S = OppoInstallAcpi (&Gic.Profile, &R);
  DEBUG ((DEBUG_INFO, "OS ACPI INSTALL=%r MEMORY=%r PUBLISH=%r READBACK=%r\n",
    S, R.MemoryStatus, R.PublishStatus, R.ReadbackStatus));
  DEBUG ((DEBUG_INFO, "OS ACPI INSTALLED=%u RSDP=%Lx PAGES=%u BYTES=%u\n",
    (UINT32)R.Published, R.Base + R.Layout.RsdpOffset, (UINT32)R.Pages, R.Layout.TotalBytes));
  if (S != EFI_SUCCESS) return S;
  if (!R.Published || R.ReadbackStatus != EFI_SUCCESS) return EFI_COMPROMISED_DATA;
  DEBUG ((DEBUG_INFO, "OS ACPI READY: SD BOOT MAY PROCEED\n"));
  return EFI_SUCCESS;
}
