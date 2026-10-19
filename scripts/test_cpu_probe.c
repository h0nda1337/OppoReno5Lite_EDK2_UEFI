/* SPDX-License-Identifier: BSD-2-Clause */
#include <Library/OppoCpuProbeLib.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

VOID *EFIAPI ZeroMem(VOID *p,UINTN n) { return memset(p,0,n); }
VOID *EFIAPI CopyMem(VOID *a,CONST VOID *b,UINTN n) { return memmove(a,b,n); }
static UINTN calls,fail,scenario,smcs,regs;
static EFI_STATUS EFIAPI reg(VOID *context,OPPO_CPU_REGISTER id,UINT64 *v)
{
  assert(context==&calls);calls++;if(calls==fail)return EFI_TIMEOUT;regs++;
  switch(id) {
    case OppoCpuEl:*v=scenario==1?0:scenario==2?8:4;break;
    case OppoCpuMpidr:*v=scenario==3?0x80000700:0x80000600;break;
    case OppoCpuMidr:*v=0x410fd0a0;break;
    case OppoCpuPfr0:*v=scenario==4?0:0x1000000;break;
    case OppoCpuSre:*v=scenario==5?0:7;break;
    default:assert(0);
  }
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI psci(VOID *context,UINT32 f,UINT64 a,UINT64 b,UINT64 *v)
{
  assert(context==&calls && b==0);calls++;if(calls==fail)return EFI_TIMEOUT;smcs++;
  if(f==ARM_SMC_ID_PSCI_VERSION){assert(a==0);*v=scenario==6?2:scenario==7?1:0x10000;return EFI_SUCCESS;}
  if(f==ARM_SMC_ID_PSCI_FEATURES) {
    assert(a==ARM_SMC_ID_PSCI_CPU_ON_AARCH64 || a==ARM_SMC_ID_PSCI_AFFINITY_INFO_AARCH64);
    *v=scenario==8?0xffffffff:scenario==9?4:0;return EFI_SUCCESS;
  }
  assert(f==ARM_SMC_ID_PSCI_AFFINITY_INFO_AARCH64 && (a&~0x700ULL)==0);
  *v=a==0x600?0:1;
  if(a==0x200 && scenario==10)*v=2;
  if(a==0x200 && scenario==11)*v=(UINT64)(INT64)-8;
  if(a==0x200 && scenario==12)*v=0xfffffff8;
  if(a==0x200 && scenario==13)*v=0x1234fffffff8ULL;
  if(a==0x200 && scenario==14)*v=3;
  if(a==0x600 && scenario==15)*v=1;
  return EFI_SUCCESS;
}
static VOID reset(UINTN n,UINTN f) { scenario=n;fail=f;calls=smcs=regs=0; }
int main(void)
{
  OPPO_ACPI_PROFILE p={.BootMpidr=0x600};p.Cpu[0].Mpidr=0x600;
  for(UINTN i=1;i<8;i++)p.Cpu[i].Mpidr=(i<=6?i-1:i)*256;
  struct { UINT8 a[16];OPPO_CPU_OBSERVATION o;UINT8 b[16]; } out,before;
  memset(&out,0xa5,sizeof(out));before=out;
  for(UINTN n=0;n<=15;n++) {
    reset(n,0);out=before;EFI_STATUS s=OppoObserveCpu(&p,reg,psci,&calls,&out.o);
    if(n==1 || n==3 || n==4 || n==7 || n==9 || n==13 || n==14) {
      assert(EFI_ERROR(s) && memcmp(&out,&before,sizeof(out))==0);
      if(n==1 || n==3 || n==4)assert(smcs==0);
      if(n==1)assert(regs==1);
      if(n==4)assert(regs==4);
    } else {
      assert(s==EFI_SUCCESS && regs==5 && smcs==(n==6?9U:11U));
      assert(out.o.CurrentEl==(n==2?8U:4U) && out.o.FeaturesQueried==(n!=6));
      assert(out.o.BootReportedOn==(n!=15) && out.o.Sre==(n==5?0U:7U));
      assert(out.o.CpuOnFeature==(n==8?-1:0));
      assert(out.o.AffinityState[3]==(n==10?2:n==11 || n==12?-8:1));
      assert(out.o.Legacy32BitErrorMask==(n==12?8:0));
      assert(memcmp(out.a,before.a,16)==0 && memcmp(out.b,before.b,16)==0);
    }
  }
  for(UINTN f=1;f<=16;f++) {
    reset(0,f);out=before;
    assert(OppoObserveCpu(&p,reg,psci,&calls,&out.o)==EFI_TIMEOUT && calls==f);
    assert(memcmp(&out,&before,sizeof(out))==0);
  }
  for(UINTN n=0;n<3;n++) {
    OPPO_ACPI_PROFILE bad=p;if(n==0)bad.Cpu[1].Mpidr=0x600;if(n==1)bad.Cpu[1].Mpidr=1;if(n==2)bad.BootMpidr=0x700;
    reset(0,0);out=before;assert(OppoObserveCpu(&bad,reg,psci,&calls,&out.o)==EFI_UNSUPPORTED);
    assert(calls==0 && memcmp(&out,&before,sizeof(out))==0);
  }
  reset(0,0);assert(OppoObserveCpu(&p,reg,psci,&calls,(VOID *)&p)==EFI_INVALID_PARAMETER);
  assert(OppoObserveCpu(NULL,reg,psci,&calls,&out.o)==EFI_INVALID_PARAMETER && calls==0);
  puts("CPU probe:16 observation scenarios,16 callback failures, invalid affinity/alias gates passed; synthetic CPU registers and PSCI only");
  return 0;
}
