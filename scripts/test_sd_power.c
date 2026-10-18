/* SPDX-License-Identifier: BSD-2-Clause */
/* Synthetic controller/slave only: does not establish physical regulator state. */
#include <Uefi.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../OppoReno5LitePkg/Library/OppoSdPowerLib/OppoSdPowerLib.c"
STATIC UINT32 Regs[64], Before[64], Scenario, Transfers, Fifo, Writes, Polls, Delays;
STATIC BOOLEAN Pending, Finishing;
STATIC UINT8 Index;
STATIC CONST UINT8 Values[] = {0x81,0x44,0xa0,0x82,0x04,0x2a};
STATIC CONST UINT8 Indices[] = {4,5,9,10,11,15};
STATIC UINT32 ReadTest (VOID *Context, UINT32 Offset)
{
  (VOID)Context;
  assert(Offset < 256 && Offset % 4 == 0);
  if(Offset == 0x24 && Scenario == 7) return 1;
  if(Offset == 0x0c && Pending) {
    Polls++;
    if(Scenario == 2 || Scenario == 9 || (Scenario == 8 && Polls % 4)) return 0;
    Pending=FALSE; Regs[0x24/4]=0; Finishing=TRUE;
    return Scenario==1 ? 2 : Scenario==6 ? 3 : 1;
  }
  if(Offset == 0) {
    UINT8 Data=Transfers==1 ? (Scenario==3 ? 0x40 : 0x51) : Values[Transfers-2];
    if(Fifo++ == 0) return Data;
    assert(Transfers>1 && Fifo==2);
    UINT8 Packet[]={0xc9,Index,Data};
    return Crc(Packet,3) ^ (Scenario==4 ? 1 : 0);
  }
  if(Offset == 0x48 && Scenario==5 && Transfers==7 && Regs[0x48/4]==Before[0x48/4])
    return 0xffff;
  return Regs[Offset/4];
}
STATIC VOID WriteTest (VOID *Context, UINT32 Offset, UINT32 Value)
{
  (VOID)Context; Writes++;
  assert(Offset < 256 && Offset % 4 == 0);
  if(Offset == 0) {assert(Fifo==0); Index=(UINT8)Value; return;}
  if(Offset == 0x0c) {Regs[Offset/4]=0;return;}
  if(Offset == 0x38) {assert(Value==1); Fifo=0;return;}
  if(Offset == 0x50) {
    assert(Value==1 && (Scenario==2 || Scenario==9));
    if(Scenario!=9) {Regs[0x24/4]=0;Pending=FALSE;} return;
  }
  Regs[Offset/4]=Value;
  if(Offset == 0x24) {
    assert(Value==1);Transfers++; Pending=TRUE; Finishing=FALSE;
    assert(Regs[0x08/4]==0 && Regs[0x10/4]==0x3a && !(Regs[0x10/4]&4));
    assert(Regs[0x14/4]==1 && Regs[0x18/4]==2); // Never a slave-value write.
    assert(Regs[0x48/4]==0x0404 && Regs[0x20/4]==0x073f && Regs[0x2c/4]==0x01ff);
    assert(Regs[0x30/4]==0 && Regs[0x34/4]==0);
    if(Transfers==1) assert(Regs[0x04/4]==0x68 && Regs[0x44/4]==1 && Index==0);
    else assert(Transfers<=7 && Regs[0x04/4]==0xc8 && Regs[0x44/4]==2 && Index==Indices[Transfers-2]);
  }
}
STATIC VOID DelayTest (VOID *Context, UINT32 Us)
{(VOID)Context;assert(Us==10);Delays++;assert(Delays<=10001);}
int main(void)
{
  OPPO_I2C_IO Io={NULL,ReadTest,WriteTest,DelayTest};OPPO_SD_POWER P;
  assert(Crc((CONST UINT8 *)"123456789",9)==0xf4); // CRC-8/SMBUS standard vector.
  assert(OppoSdPowerInspect(&Io,NULL)==EFI_INVALID_PARAMETER);
  assert(OppoSdPowerInspect(NULL,&P)==EFI_INVALID_PARAMETER);
  for(Scenario=0;Scenario<10;Scenario++) {
    for(UINT32 I=0;I<64;I++)Regs[I]=0x100+I;
    Regs[0x24/4]=0;memcpy(Before,Regs,sizeof(Regs));
    Transfers=Fifo=Writes=Polls=Delays=0;Pending=Finishing=FALSE;memset(&P,0,sizeof(P));
    EFI_STATUS Status=OppoSdPowerInspect(&Io,&P);
    if(Scenario==0 || Scenario==5 || Scenario==8) {
      assert(Status==EFI_SUCCESS && P.Valid==0x7f && Transfers==7);
      assert(P.ChipId==0x51 && P.VmcControl2==0x44 && P.VmchControl2==4);
      assert(P.Restore==(Scenario==5 ? EFI_DEVICE_ERROR : EFI_SUCCESS));
    } else if(Scenario==1 || Scenario==6) assert(Status==EFI_DEVICE_ERROR && P.Valid==0 && Transfers==1);
    else if(Scenario==2 || Scenario==9) assert(Status==EFI_TIMEOUT && P.Valid==0 && Transfers==1 && Delays==10001);
    else if(Scenario==3) assert(Status==EFI_UNSUPPORTED && P.Valid==1 && Transfers==1);
    else if(Scenario==4) assert(Status==EFI_CRC_ERROR && P.Valid==1 && Transfers==2);
    else if(Scenario==7) assert(Status==EFI_NOT_READY && Writes==0 && P.Restore==EFI_NOT_STARTED);
    if(Scenario!=7 && Scenario!=9) {
      for(UINTN I=0;I<14;I++) {
        CONST UINT32 R[]={0x04,0x08,0x10,0x14,0x18,0x1c,0x20,0x28,0x2c,0x30,0x34,0x44,0x48,0x4c};
        assert(Regs[R[I]/4]==Before[R[I]/4]);
      }
    }
    if(Scenario==9) assert(P.Restore==EFI_DEVICE_ERROR && P.RestoreRegister==0x24 && P.RestoreExpected==0 && P.RestoreObserved==1);
    else if(Scenario==5) assert(P.RestoreRegister==0x48 && P.RestoreExpected==Before[0x48/4] && P.RestoreObserved==0xffff);
    else assert(P.RestoreRegister==MAX_UINT32);
  }
  puts("SD power query: 10 synthetic scenarios, CRC-8 vector, exact read-only slave transactions, bounded errors, busy bus and controller restoration passed; no physical rail validation.");
}
