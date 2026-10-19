/* SPDX-License-Identifier: BSD-2-Clause */
/* HOB/PCD/range policy with a stub observer; all MMIO is synthetic. */
#include <PiDxe.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static BOOLEAN enabled;
static BOOLEAN install_enabled;
static BOOLEAN cpu_enabled;
#define _PCD_GET_MODE_BOOL_PcdNativeAcpiProfileLab enabled
#define _PCD_GET_MODE_BOOL_PcdNativeAcpiInstallLab install_enabled
#define _PCD_GET_MODE_BOOL_PcdNativeCpuProfileLab cpu_enabled
#include "../OppoReno5LitePkg/Drivers/OppoBdsDxe/AcpiProbe.c"
EFI_GUID gOppoAcpiHandoffHobGuid;
static struct { EFI_HOB_GUID_TYPE header; OPPO_ACPI_HANDOFF handoff; } hob;
static UINTN scenario, calls, probes, messages, install_calls, cpu_calls;
VOID OppoCpuProbeLab(CONST OPPO_ACPI_PROFILE *p) { assert(p && cpu_enabled);cpu_calls++; }
EFI_STATUS EFIAPI OppoInstallAcpi(CONST OPPO_ACPI_PROFILE *profile, OPPO_ACPI_INSTALL_REPORT *report)
{ assert(profile && report && install_enabled);install_calls++;memset(report,0,sizeof(*report));return EFI_SUCCESS; }
VOID *EFIAPI GetFirstGuidHob(CONST EFI_GUID *g)
{ assert(g==&gOppoAcpiHandoffHobGuid);return scenario==1?NULL:&hob; }
UINTN EFIAPI ArmReadMpidr(VOID) { return 0x80000600; }
UINT32 EFIAPI MmioRead32(UINTN a)
{ assert(a==0xc00ffe8 || a==0xc04ffe8 || a==0xc040008 || a==0xc04000c);calls++;return 0x3b; }
VOID EFIAPI DebugPrint(UINTN level, CONST CHAR8 *format, ...)
{ assert(level==DEBUG_INFO && format);messages++; }
BOOLEAN EFIAPI DebugPrintEnabled(VOID) { return TRUE; }
BOOLEAN EFIAPI DebugPrintLevelEnabled(CONST UINTN level) { return (BOOLEAN)(level==DEBUG_INFO); }
EFI_STATUS EFIAPI OppoAcpiObserveGic(CONST VOID *fdt, UINTN bytes, UINT64 boot,
  OPPO_GIC_READ32 read, VOID *context, OPPO_ACPI_GIC_OBSERVATION *out)
{
  assert((UINTN)fdt==0x80000000 && bytes==0x40000 && boot==0x80000600);
  probes++;UINT32 v;
  assert(read(context,0xc00ffe8,&v)==EFI_SUCCESS && v==0x3b);
  assert(read(context,0xc04ffe8,&v)==EFI_SUCCESS);
  assert(read(context,0xc040008,&v)==EFI_SUCCESS);
  assert(read(context,0xc04000c,&v)==EFI_SUCCESS);
  if(scenario==13 || scenario==15)return EFI_UNSUPPORTED;
  memset(out,0,sizeof(*out));return EFI_SUCCESS;
}
int main(void)
{
  for(scenario=0;scenario<16;scenario++) {
    enabled=TRUE;calls=probes=messages=0;memset(&hob,0,sizeof(hob));
    install_enabled=(BOOLEAN)(scenario==12 || scenario==13);install_calls=cpu_calls=0;
    cpu_enabled=(BOOLEAN)(scenario>=14);
    hob.header.Header.HobLength=sizeof(hob);
    hob.handoff=(OPPO_ACPI_HANDOFF){OPPO_ACPI_HANDOFF_SIGNATURE,OPPO_ACPI_HANDOFF_REVISION,0x80000000,0x40000,0};
    switch(scenario) {
      case 2:hob.handoff.Signature++;break;
      case 3:hob.handoff.Revision++;break;
      case 4:hob.handoff.Reserved=1;break;
      case 5:hob.handoff.FdtBase++;break;
      case 6:hob.handoff.FdtBase=OPPO_NATIVE_DRAM_BASE-8;break;
      case 7:hob.handoff.FdtBase=OPPO_NATIVE_DRAM_END-8;break;
      case 8:hob.handoff.FdtBytes=39;break;
      case 9:hob.handoff.FdtBytes=2*1024*1024+1;break;
      case 10:hob.header.Header.HobLength--;break;
      case 11:enabled=FALSE;break;
    }
    OppoAcpiProbeLab();
    assert(probes==((scenario==0 || scenario>=12)?1U:0U) && calls==((scenario==0 || scenario>=12)?4U:0U));
    assert(install_calls==(scenario==12?1U:0U));
    assert(cpu_calls==(scenario==14?1U:0U));
    assert(messages==(scenario==11?0U:scenario==12?15U:scenario==0?11U:2U));
  }
  READ_TRACE t={0,0,0};UINT32 value=0x12345678;UINT64 bad[]={0,0xc000008,0xc03ffe8,0xc040004,0xc05ffe8,0xc240008,MAX_UINT64};
  for(UINTN i=0;i<ARRAY_SIZE(bad);i++) {
    assert(ReadGic(&t,bad[i],&value)==EFI_ACCESS_DENIED);
    assert(t.Reads==0 && value==0x12345678);
  }
  puts("ACPI binding:16 HOB/PCD cases, observer failure blocks installation and CPU queries, exact register allowlist passed; no physical MMIO");
  return 0;
}
