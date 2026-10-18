/* SPDX-License-Identifier: BSD-2-Clause */
/* Original synthetic I2C/slave model, not a physical power experiment. */
#include <Uefi.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../OppoReno5LitePkg/Library/OppoSdPowerLib/OppoSdPowerLib.c"
STATIC UINT32 Controller[64], Before[64], Scenario, Transfers, SlaveWrites, Delayed;
STATIC UINT8 Rail[64], Tx[8], TxBytes, RxByte, RxValue, RxCrc;
STATIC UINT32 PendingEvents;
STATIC BOOLEAN Pending, CrcFaulted, RolledBack;

STATIC UINT8 RailValue(UINT8 R)
{
  UINT8 V=Rail[R];
  if(R==11) {
    V&=(UINT8)~4;
    if((V&0x40) && (Rail[12]&0x40) && Scenario!=7) V|=4;
  }
  if(R==5) {
    V&=(UINT8)~4;
    if((V&0x40) && (RailValue(11)&4) && Scenario!=8 && Scenario!=11 && Scenario!=12 && Scenario!=15) V|=4;
    if(Scenario==12 && RolledBack && !(V&0x40)) V|=4; // Output still high after disable.
    if(Scenario==17 && SlaveWrites==0) V|=4;
  }
  return V;
}
STATIC UINT32 ModelRead(VOID *Context,UINT32 Offset)
{
  (VOID)Context;assert(Offset<256 && Offset%4==0);
  if(Offset==0x24 && Scenario==19 && Transfers==0) return 1;
  if(Offset==0x0c && Pending) {
    if(!PendingEvents)return 0;
    Pending=FALSE;Controller[0x24/4]=0;return PendingEvents;
  }
  if(Offset==0) return RxByte++==0 ? RxValue : RxCrc;
  if(Offset==0x48 && Scenario==20 && SlaveWrites==5 && Controller[Offset/4]==Before[Offset/4]) return 0xffff;
  return Controller[Offset/4];
}
STATIC VOID ModelWrite(VOID *Context,UINT32 Offset,UINT32 Value)
{
  (VOID)Context;assert(Offset<256 && Offset%4==0 && Offset!=0x30);
  if(Offset==0) {assert(TxBytes<sizeof(Tx));Tx[TxBytes++]=(UINT8)Value;return;}
  if(Offset==0x38) {assert(Value==1);TxBytes=RxByte=0;return;}
  if(Offset==0x0c) {Controller[Offset/4]=0;return;}
  if(Offset==0x50) {assert(Value==1 && Scenario==13);Pending=FALSE;Controller[0x24/4]=0;return;}
  Controller[Offset/4]=Value;
  if(Offset!=0x24)return;
  assert(Value==1 && Controller[0x08/4]==0 && !(Controller[0x10/4]&4));
  assert(Controller[0x30/4]==0x8000 && Controller[0x48/4]==0x0404);
  Transfers++;Pending=TRUE;PendingEvents=1;
  UINT8 R=Tx[0];
  if(Controller[0x18/4]==2) {
    assert(TxBytes==1 && Controller[0x14/4]==1 && Controller[0x10/4]==0x3a);
    if(Controller[0x04/4]==0x68) {assert(R==0 && Controller[0x44/4]==1);RxValue=Scenario==1 ? 0x40 : 0x53;return;}
    assert(Controller[0x04/4]==0xc8 && Controller[0x44/4]==2);
    assert(R==4 || R==5 || R==9 || R==10 || R==11 || R==12 || R==15);
    RxValue=RailValue(R);
    if(Scenario==14 && SlaveWrites==5 && R==9) RxValue=0xab; // Final selector read disagrees.
    UINT8 Packet[3]={0xc9,R,RxValue};RxCrc=Crc(Packet,3);
    if(!CrcFaulted && ((Scenario==4 && R==12 && SlaveWrites==0) ||
       (Scenario==9 && R==9 && SlaveWrites==1) || (Scenario==16 && R==4))) {RxCrc^=1;CrcFaulted=TRUE;}
    return;
  }
  assert(Controller[0x18/4]==1 && Controller[0x14/4]==4 && Controller[0x44/4]==0);
  assert(Controller[0x10/4]==0x28 && Controller[0x04/4]==0xc8 && TxBytes==4 && Tx[3]==0);
  assert(R==5 || R==9 || R==11 || R==12 || R==15);
  UINT8 Packet[3]={0xc8,R,Tx[1]};assert(Tx[2]==Crc(Packet,3));
  SlaveWrites++;
  if((Scenario==5 && SlaveWrites==1) || (Scenario==6 && SlaveWrites==3) ||
     (Scenario==11 && R==5 && !(Tx[1]&0x40)) || (Scenario==15 && R==11 && !(Tx[1]&0x40))) {
    PendingEvents=2;return;
  }
  if(Scenario==10 && SlaveWrites==1) return; // ACK but selected value not committed.
  // The model rejects selector updates while a rail reports enabled OR live.
  if(R==9) assert(!(RailValue(5)&0x44));
  if(R==15) assert(!(RailValue(11)&0x44));
  if(R==5 && (Tx[1]&0x40)) assert((RailValue(11)&0x44)==0x44 && Rail[9]==0xaa && (Rail[15]&0x7f)==0x2a);
  if((R==5 || R==11) && !(Tx[1]&0x40) && SlaveWrites>4) RolledBack=TRUE;
  Rail[R]=Tx[1] & (UINT8)((R==5 || R==11) ? ~4U : 0xffU);
  if(Scenario==13 && SlaveWrites==1) PendingEvents=0; // Uncertain completed write, lost DONE.
}
STATIC VOID ModelDelay(VOID *Context,UINT32 Us)
{(VOID)Context;assert(Us==10 || Us==1000 || Us==20000);Delayed+=Us;assert(Delayed<1000000);}
int main(void)
{
  OPPO_I2C_IO Io={NULL,ModelRead,ModelWrite,ModelDelay};OPPO_SD_POWER P;
  assert(WriteLdo(&Io,&P,2,0)==EFI_INVALID_PARAMETER); // Reset and other rails are outside the write set.
  for(Scenario=0;Scenario<21;Scenario++) {
    for(UINT32 I=0;I<64;I++)Controller[I]=0x100+I;
    Controller[0x24/4]=0;Controller[0x30/4]=Scenario==18 ? 0x8001 : 0x8000;
    memcpy(Before,Controller,sizeof(Before));memset(Rail,0,sizeof(Rail));
    Rail[5]=Rail[11]=0x80;Rail[9]=0x80;Rail[15]=0xa5; // Preserve VMCH's unrelated high bit.
    if(Scenario==2)Rail[4]=1;
    if(Scenario==3)Rail[5]=0xc0;
    if(Scenario==17)Rail[5]=0x84;
    Transfers=SlaveWrites=Delayed=TxBytes=RxByte=0;Pending=CrcFaulted=RolledBack=FALSE;
    memset(&P,0,sizeof(P));EFI_STATUS Status=OppoSdPowerEnable(&Io,&P);
    assert(Status==(EFI_ERROR(P.Status) ? P.Status : EFI_ERROR(P.Apply) ? P.Apply : EFI_ERROR(P.Restore) ? P.Restore : P.Status));
    if(Scenario==0 || Scenario==20) {
      assert(P.Status==EFI_SUCCESS && P.Apply==EFI_SUCCESS && P.Rollback==EFI_NOT_STARTED && SlaveWrites==5);
      assert(Rail[9]==0xaa && Rail[15]==0xaa && (RailValue(5)&0x44)==0x44 && (RailValue(11)&0x44)==0x44);
      assert(P.ActiveVmcVoltage==0xaa && P.ActiveVmchVoltage==0xaa);
    } else if(Scenario==1 || Scenario==18) assert(P.Status==EFI_UNSUPPORTED && SlaveWrites==0 && P.Apply==EFI_NOT_STARTED);
    else if(Scenario==19) assert(P.Status==EFI_NOT_READY && SlaveWrites==0 && Transfers==0);
    else if(Scenario==2 || Scenario==3 || Scenario==17) assert(P.Apply==EFI_UNSUPPORTED && SlaveWrites==0);
    else if(Scenario==4) assert(P.Apply==EFI_CRC_ERROR && SlaveWrites==0 && P.Rollback==EFI_NOT_STARTED);
    else if(Scenario==16) assert(P.Status==EFI_CRC_ERROR && SlaveWrites==0 && P.Apply==EFI_NOT_STARTED);
    else {
      assert(EFI_ERROR(P.Apply) && P.ApplyFailureRegister!=MAX_UINT32);
      if(Scenario==11 || Scenario==12 || Scenario==15) {
        assert(EFI_ERROR(P.Rollback));
        if(Scenario==11 || Scenario==12) assert(Rail[9]==0xaa); // Do not retarget live VMC.
        if(Scenario==15)assert((Rail[15]&0x7f)==0x2a); // Do not retarget live VMCH.
      } else {
        assert(P.Rollback==EFI_SUCCESS && Rail[5]==0x80 && Rail[11]==0x80);
        assert(Rail[9]==0x80 && Rail[15]==0xa5 && Rail[12]==0);
      }
    }
    if(Scenario==20) assert(P.Restore==EFI_DEVICE_ERROR && P.RestoreRegister==0x48 && Status==EFI_DEVICE_ERROR);
    else if(Scenario!=18 && Scenario!=19) assert(P.Restore==EFI_SUCCESS);
    CONST UINT32 Saved[]={0x04,0x08,0x10,0x14,0x18,0x1c,0x20,0x28,0x2c,0x30,0x34,0x44,0x48,0x4c};
    for(UINT32 I=0;I<14;I++)assert(Controller[Saved[I]/4]==Before[Saved[I]/4]);
  }
  puts("SD power enable: 21 synthetic scenarios; exact CRC packets, guarded off-state voltage changes, VMCH-before-VMC, timeouts, NACK/CRC/readback faults, rollback, no live-rail retargeting and controller restoration failure passed. No hardware power validation.");
}
