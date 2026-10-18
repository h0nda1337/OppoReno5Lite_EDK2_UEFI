/* SPDX-License-Identifier: BSD-2-Clause */
#include <Library/OppoAcpiProbeLib.h>
#include <Library/BaseMemoryLib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VOID *EFIAPI ZeroMem(VOID *p, UINTN n) { return memset(p,0,n); }
VOID *EFIAPI CopyMem(VOID *a, CONST VOID *b, UINTN n) { return memmove(a,b,n); }
INTN EFIAPI CompareMem(CONST VOID *a, CONST VOID *b, UINTN n) { return memcmp(a,b,n); }
static UINTN calls, fail_at, scenario, reads[8][2];
static UINTN permutation[8] = {6,4,0,5,7,1,3,2};

static EFI_STATUS EFIAPI read32(VOID *context, UINT64 a, UINT32 *value)
{
  assert(context == &calls); calls++;
  if (calls == fail_at) return EFI_TIMEOUT;
  if (a == 0xc00ffe8) { *value = scenario == 1 ? 0x40 : 0x3b; return EFI_SUCCESS; }
  if (a == 0xc000004) { *value = 0x007a000f; return EFI_SUCCESS; }
  if (a == 0xc000000) { *value = 0x52; return EFI_SUCCESS; }
  assert(a >= 0xc040000 && a < 0xc240000);
  UINTN slot = (a-0xc040000)/0x20000, offset = (a-0xc040000)%0x20000;
  assert(slot < 8); // Never continue past the eight declared CPU frames.
  if (offset == 0xffe8) { *value = scenario == 2 && slot == 5 ? 0x20 : 0x3b; return EFI_SUCCESS; }
  assert(offset == 8 || offset == 12);
  UINTN affinity = scenario == 0 || scenario == 10 ? permutation[slot] : slot;
  if (offset == 12) {
    *value = (UINT32)(affinity << 8); reads[slot][1]++;
    if (scenario == 3 && slot == 2) *value = 0;
    if (scenario == 4 && slot == 2) *value = 0x800;
    if (scenario == 5 && slot == 3 && reads[slot][1] == 2) *value ^= 0x100;
  } else {
    *value = (UINT32)(slot << 8) | 1; reads[slot][0]++;
    if (slot == 7 && scenario != 6) *value |= BIT4;
    if (scenario == 7 && slot == 2) *value |= BIT4;
    if (scenario == 8 && slot == 2) *value |= BIT1;
    if (scenario == 9 && slot == 3 && reads[slot][0] == 2) *value ^= BIT0;
  }
  return EFI_SUCCESS;
}

static VOID reset(UINTN test, UINTN fail)
{ calls=0;scenario=test;fail_at=fail;memset(reads,0,sizeof(reads)); }

static void checks(UINT8 *fdt, UINTN bytes)
{
  struct { UINT8 left[16]; OPPO_ACPI_GIC_OBSERVATION observation; UINT8 right[16]; } guarded, before;
  memset(&guarded,0xa5,sizeof(guarded)); before=guarded;
  reset(0,0);
  assert(OppoAcpiObserveGic(fdt,bytes,0x80000600,read32,&calls,&guarded.observation)==EFI_SUCCESS);
  assert(calls==43 && guarded.observation.Profile.BootMpidr==0x600);
  assert(memcmp(before.left,guarded.left,16)==0 && memcmp(before.right,guarded.right,16)==0);
  for (UINTN uid=0;uid<8;uid++) {
    OPPO_ACPI_CPU c=guarded.observation.Profile.Cpu[uid];
    UINTN cpu=uid==0 ? 6 : uid<=6 ? uid-1 : uid;
    UINTN slot=0; while(permutation[slot]!=cpu) slot++;
    assert(c.Mpidr==cpu*256 && c.RedistributorBase==0xc040000+slot*0x20000);
    assert(guarded.observation.RedistributorTyper[uid]>>32==c.Mpidr);
    assert(guarded.observation.RedistributorPidr2[uid]==0x3b);
  }
  _Alignas(16) UINT8 tables[4096]; OPPO_ACPI_LAYOUT layout;
  assert(OppoBuildAcpiTables(&guarded.observation.Profile,tables,sizeof(tables),0x90000000,&layout)==EFI_SUCCESS);
  for (UINTN test=1;test<=9;test++) {
    guarded=before;reset(test,0);
    EFI_STATUS status=OppoAcpiObserveGic(fdt,bytes,0x80000600,read32,&calls,&guarded.observation);
    assert(status==(test==5 || test==9 ? EFI_DEVICE_ERROR : EFI_UNSUPPORTED));
    assert(calls>0 && calls<=43 && memcmp(&guarded,&before,sizeof(guarded))==0);
  }
  for (UINTN fail=1;fail<=43;fail++) {
    guarded=before;reset(0,fail);
    assert(OppoAcpiObserveGic(fdt,bytes,0x80000600,read32,&calls,&guarded.observation)==EFI_TIMEOUT);
    assert(calls==fail && memcmp(&guarded,&before,sizeof(guarded))==0);
  }
  UINT64 bad[] = {0x80000800ULL,0x80000001ULL,0x1000000000000000ULL,MAX_UINT64};
  for(UINTN i=0;i<ARRAY_SIZE(bad);i++) {
    reset(0,0);guarded=before;
    assert(OppoAcpiObserveGic(fdt,bytes,bad[i],read32,&calls,&guarded.observation)==EFI_UNSUPPORTED);
    assert(calls==0 && memcmp(&guarded,&before,sizeof(guarded))==0);
  }
  reset(0,0);
  assert(OppoAcpiObserveGic(fdt,bytes,0,NULL,&calls,&guarded.observation)==EFI_INVALID_PARAMETER);
  assert(OppoAcpiObserveGic(fdt,bytes,0,read32,&calls,NULL)==EFI_INVALID_PARAMETER);
  assert(OppoAcpiObserveGic(fdt,bytes,0,read32,&calls,(OPPO_ACPI_GIC_OBSERVATION *)fdt)==EFI_INVALID_PARAMETER);
  assert(calls==0);
}

static UINT8 *load(const char *name, UINTN *bytes)
{
  FILE *f=fopen(name,"rb");assert(f && fseek(f,0,SEEK_END)==0);
  long n=ftell(f);assert(n>0 && n<2*1024*1024 && fseek(f,0,SEEK_SET)==0);
  UINT8 *p=malloc((size_t)n);assert(p && fread(p,1,(size_t)n,f)==(size_t)n && fclose(f)==0);
  *bytes=(UINTN)n;return p;
}

int main(int argc, char **argv)
{
  assert(argc>=2);UINTN bytes;UINT8 *fdt=load(argv[1],&bytes);
  UINT8 *original=malloc(bytes);assert(original);memcpy(original,fdt,bytes);
  checks(fdt,bytes);assert(memcmp(original,fdt,bytes)==0);free(original);free(fdt);
  for(int i=2;i<argc;i++) {
    fdt=load(argv[i],&bytes);OPPO_ACPI_GIC_OBSERVATION out, before;
    memset(&out,0xa5,sizeof(out));before=out;reset(0,0);
    assert(OppoAcpiObserveGic(fdt,bytes,0x80000600,read32,&calls,&out)!=EFI_SUCCESS);
    assert(calls==0 && memcmp(&out,&before,sizeof(out))==0);free(fdt);
  }
  puts("ACPI probe: FDT gate, shuffled GICR affinities, bounded read-only callbacks,43 read failures and invalid/unstable identities passed; MMIO was synthetic");
  return 0;
}
