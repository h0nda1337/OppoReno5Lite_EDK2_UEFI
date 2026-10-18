/* SPDX-License-Identifier: BSD-2-Clause */
/* Independent synthetic SD/controller model; does not initialize real hardware. */
#include <Uefi.h>
#include <Library/OppoMsdcLib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  UINT32 Cfg, Iocon, Ps, Config, Interrupt, Argument, Response[4];
  UINT32 Opcode, Previous, Position, Attempts, Commands, Reads, Writes;
  UINT64 Sector, Delay;
  BOOLEAN Legacy, EarlyDone, NoClock, StuckReset, StuckFifo;
  BOOLEAN CommandCrc, DataCrc, DataTimeout, NoDone, OcrBusy, R1Error;
  BOOLEAN Cmd8NeedsFalling, Cmd8AlwaysCrc, Cmd8AlternateTimeout;
  UINT32 Length, CardWidth;
  BOOLEAN NoApp, ScrCrc, ScrMalformed, ScrOneBit, Cmd6Error, Cmd6Crc;
  BOOLEAN HostWidthIgnored, FastClockIgnored, FastClockStuck, FastDataCrc, Mismatch, FinalCrc;
  BOOLEAN CustomSpeed;
  UINT8 SpeedCode;
} MODEL;
STATIC MODEL M;
STATIC UINT8 Pattern(UINT64 Sector, UINT32 Offset)
{ return (UINT8)((Sector * 13 + Offset) ^ 0x5a); }
STATIC UINT32 Read(VOID *Context, UINT32 Offset)
{
  assert(Context==&M);M.Reads++;
  switch(Offset) {
    case 0x00:return M.Cfg;
    case 0x04:return M.Iocon;
    case 0x08:return M.Ps|(1U<<16);
    case 0x0c:
      if((M.Opcode==17 && M.Position>=64 && (M.DataCrc || (M.FastDataCrc && M.CardWidth==4))) ||
         (M.Opcode==51 && M.ScrCrc) || (M.Opcode==17 && M.FinalCrc && M.Position==M.Length))M.Interrupt|=1U<<15;
      if(M.Opcode==17 && M.DataTimeout)M.Interrupt|=1U<<14;
      if((M.Opcode==17 || M.Opcode==51) && !M.NoDone && (M.EarlyDone || M.Position==M.Length))M.Interrupt|=1U<<12;
      return M.Interrupt;
    case 0x14:
      if(M.StuckFifo)return 1U<<31;
      return (M.Opcode==17 || M.Opcode==51) && M.Position<M.Length ? (M.Length-M.Position<64 ? M.Length-M.Position : 64) : 0;
    case 0x1c: {
      assert((M.Opcode==17 || M.Opcode==51) && M.Position<M.Length);
      assert(M.CardWidth==(((M.Config>>16)&3)==1 ? 4U : 1U));
      UINT32 Value=0;
      for(UINT32 I=0;I<4;I++) {
        UINT32 P=M.Position+I;
        UINT8 V=M.Opcode==17 ? Pattern(M.Sector,P) : P==0 ? (M.ScrMalformed ? 0x12 : 0x02) : P==1 ? (M.ScrOneBit ? 1 : 5) : 0;
        if(M.Opcode==17 && M.Mismatch && M.CardWidth==4)V^=1;
        Value|=(UINT32)V<<(I*8);
      }
      M.Position+=4;return Value;
    }
    case 0x30:return M.Config;
    case 0x3c:return 0;
    case 0x40:case 0x44:case 0x48:case 0x4c:return M.Response[(Offset-0x40)/4];
    default:assert(!"unexpected controller read");return 0;
  }
}
STATIC VOID Write(VOID *Context, UINT32 Offset, UINT32 Value)
{
  assert(Context==&M);M.Writes++;
  switch(Offset) {
    case 0x00:
      if(M.FastClockIgnored && M.CardWidth==4 && !(Value&(0x7fffU<<8)))return;
      M.Cfg=Value & ~(1U<<7);
      if(!M.StuckReset)M.Cfg&=~(1U<<2);
      if(!M.NoClock && !(M.FastClockStuck && M.CardWidth==4))M.Cfg|=1U<<7;
      return;
    case 0x08:M.Ps=Value;return;
    case 0x04:M.Iocon=Value;return;
    case 0x0c:M.Interrupt&=~Value;return;
    case 0x10:assert(Value==0);return;
    case 0x14:assert(Value==(1U<<31));return;
    case 0x30:
      assert((Value&(3U<<16))==0 || (Value&(3U<<16))==(1U<<16));
      if(!(M.HostWidthIgnored && (Value&(1U<<16))))M.Config=Value;
      return;
    case 0x38:M.Argument=Value;return;
    case 0x50:assert(Value==1);return;
    case 0x34:break;
    default:assert(!"unexpected write, including DMA or TXDATA");return;
  }
  // The model fails immediately if a write-data direction or multiblock is used.
  assert((Value&(1U<<13))==0);
  M.Previous=M.Opcode;M.Opcode=Value&63;M.Commands++;
  memset(M.Response,0,sizeof(M.Response));M.Interrupt|=1U<<8;
  if(M.CommandCrc) {M.Interrupt|=1U<<10;return;}
  switch(M.Opcode) {
    case 0:assert(M.Argument==0 && ((Value>>7)&7)==0);M.CardWidth=1;return;
    case 8:
      assert(M.Argument==0x1aa && ((Value>>7)&7)==1);
      assert((M.Iocon & 0x00ff003dU)==0);
      if(M.Cmd8AlwaysCrc || (M.Cmd8NeedsFalling && !(M.Iocon & 2))) {
        M.Interrupt|=1U<<10;return;
      }
      if(M.Cmd8AlternateTimeout && (M.Iocon & 2)) {
        M.Interrupt=1U<<9;return;
      }
      if(M.Legacy) {M.Interrupt&=~(1U<<8);M.Interrupt|=1U<<9;}
      else M.Response[0]=0x1aa;
      return;
    case 55:assert(M.Argument==0 || M.Argument==0x12340000);M.Response[0]=M.NoApp ? 0 : 1U<<5;return;
    case 41:
      assert(M.Previous==55 && ((Value>>7)&7)==3);
      assert(M.Argument==(M.Legacy?0x00ff8000U:0x40ff8000U));
      M.Attempts++;M.Response[0]=0x00ff8000U;
      if(!M.OcrBusy && M.Attempts>=2)M.Response[0]|=0x80000000U|(M.Legacy?0:0x40000000U);
      return;
    case 2:assert(((Value>>7)&7)==2);return;
    case 3:M.Response[0]=0x12340000;return;
    case 9:
      assert(M.Argument==0x12340000 && ((Value>>7)&7)==2);
      if(M.Legacy) {M.Response[2]=(9U<<16)|255;M.Response[1]=(3U<<30)|(3U<<15);}
      else {M.Response[3]=1U<<30;M.Response[2]=1;M.Response[1]=0xffff0000U;}
      M.Response[3]|=M.CustomSpeed ? M.SpeedCode : 0x32;
      return;
    case 7:assert(M.Argument==0x12340000 && ((Value>>7)&7)==7);return;
    case 16:assert(M.Legacy && M.Argument==512);return;
    case 17:
      assert(((Value>>11)&3)==1 && (Value>>16)==512);
      if(M.Legacy)assert(M.Argument%512==0);
      M.Sector=M.Legacy?M.Argument/512:M.Argument;M.Position=0;M.Length=512;
      if(M.R1Error)M.Response[0]=1U<<30;
      return;
    case 51:
      assert(M.Previous==55 && M.Argument==0 && ((Value>>7)&7)==1);
      assert(((Value>>11)&3)==1 && (Value>>16)==8);
      M.Length=8;M.Position=0;return;
    case 6:
      assert(M.Previous==55 && M.Argument==2 && ((Value>>7)&7)==1 && ((Value>>11)&3)==0);
      if(M.Cmd6Error)M.Response[0]=1U<<30;
      else M.CardWidth=4;
      if(M.Cmd6Crc)M.Interrupt|=1U<<10;
      return;
    default:assert(!"unexpected SD command; media must never be written");
  }
}
STATIC VOID Delay(VOID *Context, UINT32 Us)
{ assert(Context==&M && Us>0 && Us<=10000);M.Delay+=Us; }
STATIC OPPO_MSDC_IO Io={&M,Read,Write,Delay};
STATIC EFI_STATUS Init(OPPO_MSDC *C, BOOLEAN Legacy)
{ memset(&M,0,sizeof(M));M.Legacy=Legacy;return OppoMsdcInitialize(C,&Io); }

int main(void)
{
  OPPO_MSDC C;
  UINT8 Buffer[1026];
  assert(Init(&C,FALSE)==EFI_SUCCESS && C.Ready && C.BlockAddressed);
  assert(C.BusWidth==1 && C.ClockHz==6500000 && C.MaximumClockHz==25000000);
  assert(C.Blocks==134217728 && M.Attempts==2); // 64 GiB synthetic SDHC CSD.
  memset(Buffer,0xcc,sizeof(Buffer));M.EarlyDone=TRUE;
  assert(OppoMsdcRead(&C,37,2,Buffer+1)==EFI_SUCCESS);
  assert(Buffer[0]==0xcc && Buffer[1025]==0xcc);
  for(UINT32 I=0;I<1024;I++)assert(Buffer[I+1]==Pattern(37+I/512,I%512));
  UINT32 Commands=M.Commands;
  assert(OppoMsdcRead(&C,C.Blocks,1,Buffer)==EFI_INVALID_PARAMETER);
  assert(OppoMsdcRead(&C,MAX_UINT64,2,Buffer)==EFI_INVALID_PARAMETER);
  assert(OppoMsdcRead(&C,C.Blocks-1,2,Buffer)==EFI_INVALID_PARAMETER);
  assert(OppoMsdcRead(&C,0,MAX_UINTN,Buffer)==EFI_INVALID_PARAMETER);
  assert(OppoMsdcRead(&C,0,1,NULL)==EFI_INVALID_PARAMETER);
  assert(OppoMsdcRead(&C,0,0,NULL)==EFI_SUCCESS && M.Commands==Commands);
  assert(Init(&C,TRUE)==EFI_SUCCESS && !C.BlockAddressed && C.Blocks==32768);
  assert(OppoMsdcRead(&C,3,1,Buffer)==EFI_SUCCESS && M.Argument==1536);
  for(UINT32 I=0;I<512;I++)assert(Buffer[I]==Pattern(3,I));
  for(UINT32 Scenario=0;Scenario<5;Scenario++) {
    assert(Init(&C,FALSE)==EFI_SUCCESS);
    M.DataCrc=Scenario==0;M.DataTimeout=Scenario==1;M.NoDone=Scenario==2;
    M.R1Error=Scenario==3;M.CommandCrc=Scenario==4;
    EFI_STATUS Expected=Scenario==0 || Scenario==4?EFI_CRC_ERROR:Scenario==3?EFI_DEVICE_ERROR:EFI_TIMEOUT;
    assert(OppoMsdcRead(&C,0,1,Buffer)==Expected && !C.Ready);
    Commands=M.Commands;
    assert(OppoMsdcRead(&C,0,1,Buffer)==EFI_NOT_READY && M.Commands==Commands);
    assert(M.Delay<2000000);
  }
  for(UINT32 Scenario=0;Scenario<5;Scenario++) {
    memset(&M,0,sizeof(M));M.NoClock=Scenario==0;M.StuckReset=Scenario==1;
    M.StuckFifo=Scenario==2;M.CommandCrc=Scenario==3;M.OcrBusy=Scenario==4;
    assert(EFI_ERROR(OppoMsdcInitialize(&C,&Io)) && !C.Ready);
    assert(M.Delay<3000000 && M.Attempts<=200);
  }
  assert(OppoMsdcInitialize(NULL,&Io)==EFI_INVALID_PARAMETER);
  // Dirty inherited timing is cleared without changing unrelated write fields.
  memset(&M,0,sizeof(M));M.Iocon=0x00ff033f;
  assert(OppoMsdcInitialize(&C,&Io)==EFI_SUCCESS && C.Cmd8Attempts==1);
  assert(C.InitialIocon==0x00ff033f && M.Iocon==0x300);
  memset(&M,0,sizeof(M));M.Cmd8NeedsFalling=TRUE;
  assert(OppoMsdcInitialize(&C,&Io)==EFI_SUCCESS && C.Cmd8Attempts==2 && M.Iocon==2);
  assert(C.Cmd8FirstStatus==EFI_CRC_ERROR && OppoMsdcRead(&C,0,1,Buffer)==EFI_SUCCESS);
  memset(&M,0,sizeof(M));M.Cmd8AlwaysCrc=TRUE;
  assert(OppoMsdcInitialize(&C,&Io)==EFI_CRC_ERROR && !C.Ready && C.Cmd8Attempts==2);
  assert(M.Opcode==8 && M.Attempts==0 && M.Delay<500000);
  memset(&M,0,sizeof(M));M.Cmd8NeedsFalling=TRUE;M.Cmd8AlternateTimeout=TRUE;
  assert(OppoMsdcInitialize(&C,&Io)==EFI_TIMEOUT && !C.Ready && M.Attempts==0);
  OPPO_MSDC_IO Bad=Io;Bad.Read32=NULL;
  assert(OppoMsdcInitialize(&C,&Bad)==EFI_INVALID_PARAMETER);
  // Four-bit default SDR has no media-write or DMA command in the model.
  for(UINTN legacy=0;legacy<2;legacy++) {
    assert(Init(&C,(BOOLEAN)legacy)==EFI_SUCCESS);
    assert(OppoMsdcEnableFastRead(&C)==EFI_SUCCESS && C.Ready && C.FastSamplesVerified);
    assert(C.BusWidth==4 && C.ClockHz==13000000 && M.CardWidth==4 && ((M.Config>>16)&3)==1);
    assert(OppoMsdcRead(&C,37,2,Buffer)==EFI_SUCCESS);
    Commands=M.Commands;
    assert(OppoMsdcEnableFastRead(&C)==EFI_ALREADY_STARTED && M.Commands==Commands);
  }
  for(UINT32 Scenario=0;Scenario<11;Scenario++) {
    assert(Init(&C,FALSE)==EFI_SUCCESS);
    M.NoApp=Scenario==0;M.ScrCrc=Scenario==1;M.ScrMalformed=Scenario==2;M.ScrOneBit=Scenario==3;
    M.Cmd6Error=Scenario==4;M.Cmd6Crc=Scenario==5;M.HostWidthIgnored=Scenario==6;
    M.FastClockIgnored=Scenario==7;M.FastClockStuck=Scenario==8;M.FastDataCrc=Scenario==9;M.Mismatch=Scenario==10;
    EFI_STATUS Expected=Scenario==1 || Scenario==5 || Scenario==9 ? EFI_CRC_ERROR :
      Scenario==2 || Scenario==3 ? EFI_UNSUPPORTED : Scenario==8 ? EFI_TIMEOUT :
      Scenario==10 ? EFI_COMPROMISED_DATA : EFI_DEVICE_ERROR;
    assert(OppoMsdcEnableFastRead(&C)==Expected && !C.Ready && !C.FastSamplesVerified);
    Commands=M.Commands;
    assert(OppoMsdcRead(&C,0,1,Buffer)==EFI_NOT_READY && M.Commands==Commands && M.Delay<2000000);
  }
  assert(Init(&C,FALSE)==EFI_SUCCESS);C.MaximumClockHz=10000000;Commands=M.Commands;
  assert(OppoMsdcEnableFastRead(&C)==EFI_UNSUPPORTED && C.Ready && M.Commands==Commands);
  assert(Init(&C,FALSE)==EFI_SUCCESS);M.EarlyDone=M.FinalCrc=TRUE;
  assert(OppoMsdcRead(&C,0,1,Buffer)==EFI_CRC_ERROR && !C.Ready);
  assert(OppoMsdcEnableFastRead(NULL)==EFI_NOT_READY);
  STATIC CONST struct {UINT8 Code;UINT32 Hz;} Rates[]={{0x32,25000000},{0x5a,50000000},
    {0x1a,13000000},{0x0a,10000000},{0x7b,800000000},{0x00,0},{0x84,0},{0x34,0}};
  for(UINTN I=0;I<ARRAY_SIZE(Rates);I++) {
    memset(&M,0,sizeof(M));M.CustomSpeed=TRUE;M.SpeedCode=Rates[I].Code;
    assert(OppoMsdcInitialize(&C,&Io)==EFI_SUCCESS && C.MaximumClockHz==Rates[I].Hz);
  }
  puts("Original MSDC: SDHC/SDSC and 4-bit/13MHz sample comparisons, 11 negotiation faults, late CRC, 8 CSD speed encodings and legacy bounds passed sanitizers; no media-write/DMA commands or physical throughput measured.");
  return 0;
}
