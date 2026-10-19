/* SPDX-License-Identifier: BSD-2-Clause */
/* ACPI publication transaction policy before SD dispatch; synthetic callbacks. */
#include <Uefi.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static BOOLEAN enabled;
#define _PCD_GET_MODE_BOOL_PcdNativeSdAcpiBootLab enabled
#include "../OppoReno5LitePkg/Drivers/OppoBdsDxe/AcpiBoot.c"
static UINTN scenario, step;
EFI_STATUS OppoReadNativeAcpiProfile(OPPO_ACPI_GIC_OBSERVATION *o)
{ assert(++step==1);memset(o,0,sizeof(*o));o->Profile.BootMpidr=0x600;return scenario==1?EFI_COMPROMISED_DATA:scenario==7?EFI_WARN_UNKNOWN_GLYPH:EFI_SUCCESS; }
EFI_STATUS OppoCpuCheckForOs(CONST OPPO_ACPI_PROFILE *p)
{ assert(++step==2 && p->BootMpidr==0x600);return scenario==2?EFI_UNSUPPORTED:EFI_SUCCESS; }
EFI_STATUS EFIAPI OppoInstallAcpi(CONST OPPO_ACPI_PROFILE *p,OPPO_ACPI_INSTALL_REPORT *r)
{
  assert(++step==3 && p->BootMpidr==0x600);r->Published=(BOOLEAN)(scenario!=4);
  r->ReadbackStatus=scenario==5?EFI_COMPROMISED_DATA:EFI_SUCCESS;
  return scenario==3?EFI_OUT_OF_RESOURCES:EFI_SUCCESS;
}
VOID EFIAPI DebugPrint(UINTN l,CONST CHAR8 *f,...) { assert(l==DEBUG_INFO && f); }
BOOLEAN EFIAPI DebugPrintEnabled(VOID) { return TRUE; }
BOOLEAN EFIAPI DebugPrintLevelEnabled(CONST UINTN l) { return (BOOLEAN)(l==DEBUG_INFO); }
int main(void)
{
  for(scenario=0;scenario<8;scenario++) {
    step=0;enabled=(BOOLEAN)(scenario!=6);EFI_STATUS s=OppoAcpiPrepareForOs();
    assert((s==EFI_SUCCESS)==(scenario==0));
    assert(step==(scenario==6?0U:scenario==1 || scenario==7?1U:scenario==2?2U:3U));
  }
  puts("ACPI OS transaction:8 cases; profile/CPU/publication/readback failures and disabled mode block SD permission; host callbacks only");
  return 0;
}
