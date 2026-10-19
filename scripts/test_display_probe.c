/* SPDX-License-Identifier: BSD-2-Clause */
#include "DisplayProbe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  UINT32 Reads, FailAt, Writes, SourceReads;
  BOOLEAN AllGated, Captured, Changed, Race, BadReadback, WriteError;
  UINT64 BadAddress;
} MODEL;
static EFI_STATUS EFIAPI read32(VOID *context, UINT64 address, UINT32 *value)
{
  MODEL *m = context;
  m->Reads++;
  if (m->FailAt && m->Reads == m->FailAt) return EFI_DEVICE_ERROR;
  if (m->Captured) {
    /* Only gate bits20..22 model the photo; unrelated clock bits are synthetic. */
    if (address == 0x14000100) *value = 0x334fffe0U;
    else {
      assert(address >= 0x14008000 && address < 0x1400a000 && !(address & 3));
      UINT32 bank=(UINT32)((address-0x14008000)/0x1000), offset=(UINT32)(address&0xfff);
      switch(offset) {
        case 0x00c: *value=1; break;
        case 0x02c:
          *value=bank==0 ? (m->Changed ? 0x10U : 0x12U) : 1U;
          if (bank==0) {
            m->SourceReads++;
            if (m->Race && m->SourceReads==2) *value^=1;
            if (m->BadReadback && m->Changed) *value^=1;
          }
          break;
        case 0x324: *value=0; break;
        case 0x024: *value=0x07000001; break;
        case 0x020: *value=0x09600438; break;
        default:
          if (bank==0) {
            switch(offset) {
              case 0x050: *value=0x21ff; break;
              case 0xf60: *value=0x76f16000; break;
              case 0x058: *value=0x09600438; break;
              case 0x05c: *value=0; break;
              case 0x064: *value=0x21ff10e0; break;
              default: *value=0; break; /* Unrecognized enabled slot in a rejected profile. */
            }
          } else {
            switch(offset) {
              case 0x030: *value=0x31ff; break;
              case 0xf40: *value=0x76520000; break;
              case 0x038: *value=0x09600438; break;
              case 0x03c: *value=0; break;
              case 0x044: *value=0x21ff1100; break;
              default: *value=0; break;
            }
          }
      }
    }
    if (address==m->BadAddress) *value^=address==0x14000100 ? (1U<<20) : 1U;
    return EFI_SUCCESS;
  }
  if (address == 0x14000100) { *value = m->AllGated ? 0x700000U : 0x400000U; return EFI_SUCCESS; }
  /* Any access to the gated third bank fails the test. */
  assert(address >= 0x14008000 && address < 0x1400a000 && !(address & 3));
  UINT32 bank = (UINT32)((address - 0x14008000) / 0x1000);
  UINT32 offset = (UINT32)(address & 0xfff);
  switch (offset) {
    case 0x00c: *value = 1; break;
    case 0x02c: *value = bank == 0 ? 9 : 1; break;
    case 0x324: *value = bank == 0 ? 2 : 0; break;
    case 0x024: *value = 0x123; break;
    case 0x020: *value = (2400U << 16) | 1080U; break;
    default: {
      /* Only the enabled L0/L3/E1 of bank0 and L0 of bank1 may be sampled. */
      UINT32 allowed[] = {0x030,0x038,0x03c,0x044,0xf40,
                          0x090,0x098,0x09c,0x0a4,0xfa0,
                          0x350,0x358,0x35c,0x364,0xfb4};
      BOOLEAN found = FALSE;
      for (UINTN i=0; i<sizeof(allowed)/sizeof(allowed[0]); i++)
        if (offset == allowed[i] && (bank == 0 || i < 5)) found=TRUE;
      assert(found);
      *value = offset == 0xf40 ? 0x76520000U : 0xabcdef00U | offset;
    }
  }
  return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI write32(VOID *context, UINT64 address, UINT32 value)
{
  MODEL *m=context;
  assert(m->Captured && address==0x1400802c && value==0x10 && !m->Changed);
  m->Writes++;
  if (m->WriteError) return EFI_WRITE_PROTECTED;
  m->Changed=TRUE;
  return EFI_SUCCESS;
}

static void cleanup_checks(OPPO_FDT_VIEW *view)
{
  OPPO_DISPLAY_CLEAN_RESULT result;
  MODEL m={.Captured=TRUE};
  assert(OppoDisplayHideInherited(view,read32,write32,&m,&result)==EFI_SUCCESS);
  assert(m.Reads==23 && m.Writes==1 && result.Attempted && result.Before==0x12 && result.After==0x10);
  OPPO_DISPLAY_SNAPSHOT after;
  assert(OppoDisplayReadSnapshot(view,read32,&m,&after)==EFI_SUCCESS);
  assert(after.Banks[0].Sources==0x10 && after.Banks[1].Sources==1);
  assert(after.Banks[1].Layers[0].Address==0x76520000 && after.Banks[1].Layers[0].Pitch==0x21ff1100);
  UINT64 critical[]={0x14000100,0x1400800c,0x1400802c,0x14008324,0x14008024,0x14008020,
                     0x14008050,0x14008f60,0x14008058,0x1400805c,0x14008064,
                     0x1400900c,0x1400902c,0x14009324,0x14009024,0x14009020,
                     0x14009030,0x14009f40,0x14009038,0x1400903c,0x14009044};
  for (UINTN i=0;i<sizeof(critical)/sizeof(critical[0]);i++) {
    m=(MODEL){.Captured=TRUE,.BadAddress=critical[i]};
    assert(OppoDisplayHideInherited(view,read32,write32,&m,&result)==EFI_UNSUPPORTED);
    assert(m.Writes==0 && !result.Attempted);
  }
  for (UINT32 i=1;i<=23;i++) {
    m=(MODEL){.Captured=TRUE,.FailAt=i};
    assert(OppoDisplayHideInherited(view,read32,write32,&m,&result)==EFI_DEVICE_ERROR);
    assert(m.Writes==(i==23 ? 1U : 0U));
  }
  m=(MODEL){.Captured=TRUE,.Race=TRUE};
  assert(OppoDisplayHideInherited(view,read32,write32,&m,&result)==EFI_ABORTED && m.Writes==0);
  m=(MODEL){.Captured=TRUE,.WriteError=TRUE};
  assert(OppoDisplayHideInherited(view,read32,write32,&m,&result)==EFI_WRITE_PROTECTED && m.Writes==1 && !m.Changed);
  m=(MODEL){.Captured=TRUE,.BadReadback=TRUE};
  assert(OppoDisplayHideInherited(view,read32,write32,&m,&result)==EFI_DEVICE_ERROR && m.Writes==1);
  m=(MODEL){0};
  assert(OppoDisplayHideInherited(view,read32,write32,&m,&result)==EFI_UNSUPPORTED && m.Writes==0);
}

static void checks(const char *file)
{
  FILE *f=fopen(file,"rb"); assert(f && fseek(f,0,SEEK_END)==0);
  long size=ftell(f); assert(size>40 && size<=2*1024*1024 && fseek(f,0,SEEK_SET)==0);
  UINT8 *data=malloc((size_t)size), *changed=malloc((size_t)size); assert(data && changed);
  assert(fread(data,1,(size_t)size,f)==(size_t)size); fclose(f);
  OPPO_FDT_VIEW view;
  OPPO_DISPLAY_SNAPSHOT snapshot, zero={0};
  MODEL model={0};
  assert(OppoFdtOpen(data,(UINTN)size,&view));
  cleanup_checks(&view);
  assert(OppoDisplayReadSnapshot(&view,read32,&model,&snapshot)==EFI_SUCCESS);
  assert(model.Reads==31 && snapshot.Banks[2].ClockOff==1);
  assert(snapshot.Banks[0].Layers[0].Address==0x76520000 && snapshot.Banks[0].Layers[5].Enabled==1);
  assert(snapshot.Banks[0].Layers[1].Enabled==0 && snapshot.Banks[1].Layers[3].Enabled==0);
  for (UINT32 i=1;i<=31;i++) {
    model=(MODEL){.FailAt=i}; memset(&snapshot,0xff,sizeof(snapshot));
    assert(OppoDisplayReadSnapshot(&view,read32,&model,&snapshot)==EFI_DEVICE_ERROR);
    assert(model.Reads==i && memcmp(&snapshot,&zero,sizeof(zero))==0);
  }
  model=(MODEL){.AllGated=TRUE};
  assert(OppoDisplayReadSnapshot(&view,read32,&model,&snapshot)==EFI_SUCCESS && model.Reads==1);
  const char *paths[]={"/","/soc","/soc/clock-controller@14000000","/soc/disp_ovl0@14008000",
                       "/soc/disp_ovl0_2l@14009000","/soc/disp_ovl1_2l@1400a000"};
  const char *properties[]={"compatible","#address-cells","compatible","reg","compatible","reg"};
  for (UINTN i=0;i<6;i++) {
    CONST VOID *value; UINT32 bytes;
    assert(OppoFdtProperty(&view,paths[i],properties[i],&value,&bytes)==EFI_SUCCESS && bytes>0);
    memcpy(changed,data,(size_t)size);
    changed[(const UINT8 *)value-data+bytes-1]^=1;
    OPPO_FDT_VIEW altered; assert(OppoFdtOpen(changed,(UINTN)size,&altered));
    model=(MODEL){0}; memset(&snapshot,0xff,sizeof(snapshot));
    assert(OppoDisplayReadSnapshot(&altered,read32,&model,&snapshot)==EFI_UNSUPPORTED);
    assert(model.Reads==0 && memcmp(&snapshot,&zero,sizeof(zero))==0);
  }
  model=(MODEL){0};
  assert(OppoDisplayReadSnapshot(NULL,read32,&model,&snapshot)==EFI_INVALID_PARAMETER && model.Reads==0);
  assert(OppoDisplayReadSnapshot(&view,NULL,&model,&snapshot)==EFI_INVALID_PARAMETER);
  assert(OppoDisplayReadSnapshot(&view,read32,&model,NULL)==EFI_INVALID_PARAMETER);
  free(data); free(changed);
}
int main(int argc,char **argv)
{
  assert(argc==2 || argc==3);
  checks(argv[1]); if(argc==3) checks(argv[2]);
  puts("Display observer: invalid bindings perform no reads; gated banks/disabled layers skipped; 31 read failures clear output. Model only; no display hardware written.");
}
