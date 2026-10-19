/* SPDX-License-Identifier: BSD-2-Clause */
#include <Uefi.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static BOOLEAN enabled=TRUE;
#define _PCD_GET_MODE_BOOL_PcdNativeCpuProfileLab enabled
#include "../OppoReno5LitePkg/Drivers/OppoBdsDxe/CpuProbe.c"
static UINTN reg_calls, smc_calls, messages;
static UINT64 current_el=4;
UINT64 EFIAPI OppoCpuCurrentEl(VOID) {reg_calls++;return current_el;}
UINT64 EFIAPI OppoCpuMpidrValue(VOID) {reg_calls++;return 0x80000000;}
UINT64 EFIAPI OppoCpuMidrValue(VOID) {reg_calls++;return 0x410fd050;}
UINT64 EFIAPI OppoCpuPfr0Value(VOID) {reg_calls++;return 0x1000000;}
UINT64 EFIAPI OppoCpuSreEl1(VOID) {assert(current_el==4);reg_calls++;return 7;}
UINT64 EFIAPI OppoCpuSreEl2(VOID) {assert(current_el==8);reg_calls++;return 15;}
VOID EFIAPI ArmCallSmc(ARM_SMC_ARGS *a)
{
  assert(a->Arg2==0 && a->Arg3==0 && a->Arg4==0 && a->Arg5==0 && a->Arg6==0 && a->Arg7==0);
  smc_calls++;
  if(a->Arg0==ARM_SMC_ID_PSCI_VERSION){assert(a->Arg1==0);a->Arg0=0x10000;}
  else if(a->Arg0==ARM_SMC_ID_PSCI_FEATURES){assert(a->Arg1==ARM_SMC_ID_PSCI_CPU_ON_AARCH64 || a->Arg1==ARM_SMC_ID_PSCI_AFFINITY_INFO_AARCH64);a->Arg0=0;}
  else {assert(a->Arg0==ARM_SMC_ID_PSCI_AFFINITY_INFO_AARCH64 && (a->Arg1&~0x700ULL)==0);a->Arg0=a->Arg1?1:0;}
}
VOID *EFIAPI ZeroMem(VOID *p,UINTN n) {return memset(p,0,n);}
VOID *EFIAPI CopyMem(VOID *a,CONST VOID *b,UINTN n) {return memmove(a,b,n);}
VOID EFIAPI DebugPrint(UINTN level,CONST CHAR8 *f,...) {assert(level==DEBUG_INFO && f);messages++;}
BOOLEAN EFIAPI DebugPrintEnabled(VOID) {return TRUE;}
BOOLEAN EFIAPI DebugPrintLevelEnabled(CONST UINTN l) {return (BOOLEAN)(l==DEBUG_INFO);}
int main(void)
{
  OPPO_ACPI_PROFILE p={0};for(UINTN i=0;i<8;i++)p.Cpu[i].Mpidr=i*256;
  for(UINTN test=0;test<3;test++) {
    enabled=(BOOLEAN)(test!=2);current_el=test==1?8:4;reg_calls=smc_calls=messages=0;
    OppoCpuProbeLab(&p);
    assert(reg_calls==(enabled?6U:0U) && smc_calls==(enabled?11U:0U) && messages==(enabled?13U:0U));
  }
  CPU_TRACE t={0};UINT64 value=0x12345678;smc_calls=reg_calls=0;
  UINT32 denied[]={ARM_SMC_ID_PSCI_CPU_ON_AARCH64,ARM_SMC_ID_PSCI_CPU_OFF,
    ARM_SMC_ID_PSCI_SYSTEM_OFF,ARM_SMC_ID_PSCI_SYSTEM_RESET,ARM_SMC_ID_PSCI_CPU_SUSPEND_AARCH64};
  for(UINTN i=0;i<ARRAY_SIZE(denied);i++)assert(ReadPsci(&t,denied[i],0,0,&value)==EFI_ACCESS_DENIED);
  assert(ReadPsci(&t,ARM_SMC_ID_PSCI_VERSION,1,0,&value)==EFI_ACCESS_DENIED);
  assert(ReadPsci(&t,ARM_SMC_ID_PSCI_FEATURES,ARM_SMC_ID_PSCI_SYSTEM_RESET,0,&value)==EFI_ACCESS_DENIED);
  assert(ReadPsci(&t,ARM_SMC_ID_PSCI_AFFINITY_INFO_AARCH64,0x800,0,&value)==EFI_ACCESS_DENIED);
  assert(ReadPsci(&t,ARM_SMC_ID_PSCI_AFFINITY_INFO_AARCH64,1,0,&value)==EFI_ACCESS_DENIED);
  assert(ReadPsci(&t,ARM_SMC_ID_PSCI_AFFINITY_INFO_AARCH64,0,1,&value)==EFI_ACCESS_DENIED);
  assert(ReadRegister(&t,(OPPO_CPU_REGISTER)99,&value)==EFI_ACCESS_DENIED);
  assert(smc_calls==0 && reg_calls==0 && t.Smcs==0 && t.Registers==0 && value==0x12345678);
  puts("CPU native binding:EL1/EL2 and disabled dispatch; exact SMC/register allowlists deny all state-changing PSCI calls; host stubs only");
  return 0;
}
