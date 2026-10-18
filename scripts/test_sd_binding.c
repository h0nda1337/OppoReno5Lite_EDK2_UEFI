/* SPDX-License-Identifier: BSD-2-Clause */
/* Board binding with synthetic HOB, registers and card; never physical MMIO. */
#include <PiDxe.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../OppoReno5LitePkg/Drivers/OppoSdDxe/OppoSdDxe.c"

EFI_GUID gOppoBootInfoHobGuid, gOppoSdDeviceGuid, gOppoSdStatusProtocolGuid;
EFI_GUID gEfiBlockIoProtocolGuid, gEfiDevicePathProtocolGuid;
STATIC EFI_BOOT_SERVICES mBoot;
EFI_BOOT_SERVICES *gBS = &mBoot;
STATIC struct {EFI_HOB_GUID_TYPE Header; OPPO_BOOT_INFO Info;} mHob;
STATIC UINTN mScenario, mReads, mWrites, mCardInits, mCardReads, mInstalls, mTpl;
STATIC CONST UINT32 Gates = (1U << 4) | (1U << 16);
STATIC UINT32 mIes, mSmt, mPupd, mR0, mR1;
STATIC UINT32 mPowerQueries, mI2cGate;
UINT8 EFIAPI MmioRead8 (UINTN A) {(VOID)A;assert(!"I2C without power profile");return 0;}
UINT16 EFIAPI MmioRead16 (UINTN A) {(VOID)A;assert(!"I2C without power profile");return 0;}
UINT8 EFIAPI MmioWrite8 (UINTN A,UINT8 V) {(VOID)A;(VOID)V;assert(!"I2C without power profile");return 0;}
UINT16 EFIAPI MmioWrite16 (UINTN A,UINT16 V) {(VOID)A;(VOID)V;assert(!"I2C without power profile");return 0;}
EFI_STATUS OppoSdPowerInspect (CONST OPPO_I2C_IO *Io, OPPO_SD_POWER *Power)
{
  assert(mScenario>=14 && Io->Read==I2cRead && Io->Write==I2cWrite);
  mPowerQueries++;Power->Status=EFI_SUCCESS;
  Power->Restore=mScenario==18 ? EFI_DEVICE_ERROR : EFI_SUCCESS;
  Power->Valid=0x7f;return EFI_SUCCESS;
}

VOID *EFIAPI GetFirstGuidHob (CONST EFI_GUID *Guid)
{ assert(Guid == &gOppoBootInfoHobGuid); return mScenario == 1 ? NULL : &mHob; }
UINT32 EFIAPI MmioRead32 (UINTN Address)
{
  mReads++;
  switch (Address) {
    case 0x10005600: return mScenario == 4 ? 1U << 9 : 0;
    case 0x10005400: return mScenario == 3 ? 0 : 0x11000000;
    case 0x10005410: return 0x1111;
    case 0x10005310: return 0x11111100;
    case 0x10005330: return mScenario==15 ? 0 : 0x00110000;
    case 0x100000b0: return mScenario==16 ? 0x80 : 2;
    case 0x100010ac: return mI2cGate;
    case 0x10000080: return mScenario == 5 ? 0x80 : 0;
    case 0x10001094: return mScenario == 6 ? Gates : 0;
    case 0x11240000: return 0x18b;
    case 0x11240004: return 0;
    case 0x11240008: return 1U << 16;
    case 0x112400f0: return 0;
    case 0x11c20030: return mIes;
    case 0x11c200c0: return mSmt;
    case 0x11c20060: return mPupd;
    case 0x11c20080: return mR0;
    case 0x11c20090: return mR1;
    default: assert(!"unapproved register read"); return 0;
  }
}
UINT32 EFIAPI MmioWrite32 (UINTN Address, UINT32 Value)
{
  mWrites++;
  switch (Address) {
    case 0x100010a8: assert(mScenario>=14 && Value==(1U<<18));if(mScenario!=17)mI2cGate=0;return Value;
    case 0x100010a4: assert(mScenario>=14 && Value==(1U<<18));if(mScenario!=19)mI2cGate=Value;return Value;
    case 0x11c20030: assert(Value==0x800000fcU || ((mScenario==10 || mScenario==13) && Value==0x80000000U));if(mScenario!=10 && mScenario!=13)mIes=Value;return Value;
    case 0x11c200c0: assert(Value==0x8000001dU || ((mScenario==10 || mScenario==13) && Value==0x80000001U));if(!(mScenario==13 && Value==0x80000001U))mSmt=Value;return Value;
    case 0x11c20060: assert(Value==0xdeadffc1U || ((mScenario==10 || mScenario==13) && Value==0xdeadffffU));mPupd=Value;return Value;
    case 0x11c20080: assert(Value==0xcafe00c0U || ((mScenario==10 || mScenario==13) && Value==0xcafe00ffU));mR0=Value;return Value;
    case 0x11c20090: assert(Value==0x233fU || ((mScenario==10 || mScenario==13) && Value==0x2300U));mR1=Value;return Value;
  }
  assert((Address == 0x10000088 && Value == 0x87) ||
         (Address == 0x10000004 && Value == 1U << 24) ||
         (Address == 0x1000108c && Value == Gates));
  return Value;
}
UINTN EFIAPI MicroSecondDelay (UINTN Us) { assert(Us <= 100); return Us; }
EFI_STATUS OppoMsdcInitialize (OPPO_MSDC *Card, CONST OPPO_MSDC_IO *Io)
{
  assert(Io->Read32 == Read32 && Io->Write32 == Write32 && Io->DelayUs == DelayUs);
  mCardInits++; Card->Blocks=8192; Card->LastCommand=8; Card->LastInterrupt=512;
  Card->Ready=mScenario != 7;
  return Card->Ready ? EFI_SUCCESS : EFI_TIMEOUT;
}
EFI_STATUS OppoMsdcRead (OPPO_MSDC *Card, UINT64 Lba, UINTN Blocks, VOID *Buffer)
{
  assert(mTpl==1 && Card == &mCard && Buffer != NULL); mCardReads++;
  if (Lba >= Card->Blocks || Blocks > Card->Blocks - Lba) return EFI_INVALID_PARAMETER;
  if (mScenario == 9) {Card->Ready=FALSE; return EFI_CRC_ERROR;}
  return EFI_SUCCESS;
}
STATIC EFI_TPL EFIAPI Raise (EFI_TPL Tpl) {assert(Tpl==TPL_CALLBACK && mTpl==0);mTpl++;return TPL_APPLICATION;}
STATIC VOID EFIAPI Restore (EFI_TPL Tpl) {assert(Tpl==TPL_APPLICATION && mTpl==1);mTpl--;}
STATIC EFI_STATUS EFIAPI Install (EFI_HANDLE *Handle, ...)
{
  va_list Args; va_start(Args,Handle);
  if (mInstalls++ == 0) {
    assert(va_arg(Args,EFI_GUID *)==&gOppoSdStatusProtocolGuid);
    assert(va_arg(Args,VOID *)==&mStatus);
    assert(va_arg(Args,VOID *)==NULL); *Handle=(VOID *)1;
  } else {
    assert(va_arg(Args,EFI_GUID *)==&gEfiBlockIoProtocolGuid);
    assert(va_arg(Args,VOID *)==&mBlock);
    assert(va_arg(Args,EFI_GUID *)==&gEfiDevicePathProtocolGuid);
    assert(va_arg(Args,VOID *)==&mPath);
    assert(va_arg(Args,VOID *)==NULL);
  }
  va_end(Args); return EFI_SUCCESS;
}
int main(void)
{
  mBoot.InstallMultipleProtocolInterfaces=Install;
  mBoot.RaiseTPL=Raise; mBoot.RestoreTPL=Restore;
  for (mScenario=0;mScenario<21;mScenario++) {
    memset(&mCard,0,sizeof(mCard)); memset(&mStatus,0,sizeof(mStatus));
    memset(&mMedia,0,sizeof(mMedia)); memset(&mHob,0,sizeof(mHob));
    mHob.Header.Header.HobLength=sizeof(mHob);
    mHob.Info.Signature=OPPO_BOOT_INFO_SIGNATURE;
    mHob.Info.Revision=OPPO_BOOT_INFO_REVISION;
    mHob.Info.Flags=mScenario==2 ? 0 : mScenario==11 ? OPPO_BOOT_SD_PROFILE_VALIDATED : mScenario==12 ? OPPO_BOOT_SD_PROFILE_VALIDATED | OPPO_BOOT_SD_PAD_BANK_VALIDATED :
      OPPO_BOOT_SD_PROFILE_VALIDATED | OPPO_BOOT_SD_PAD_BANK_VALIDATED | OPPO_BOOT_SD_RM_ROLE_VALIDATED;
    if(mScenario>=14 && mScenario<20) mHob.Info.Flags|=OPPO_BOOT_SD_POWER_PROFILE_VALIDATED;
    mIes=0x80000000U;mSmt=0x80000001U;mPupd=0xdeadffffU;mR0=0xcafe00ffU;mR1=0x2300U;
    mReads=mWrites=mCardInits=mCardReads=mInstalls=mTpl=0;
    mPowerQueries=0;mI2cGate=1U<<18;
    assert(OppoSdEntry(NULL,NULL)==EFI_SUCCESS);
    if (mScenario==1 || mScenario==2 || mScenario==11 || mScenario==12) assert(mReads==0 && mWrites==0 && mCardInits==0);
    else if (mScenario==3) assert(mWrites==0 && mCardInits==0 && mStatus.Board==EFI_NOT_READY);
    else if (mScenario==4) assert(mWrites==0 && mCardInits==0 && mStatus.Board==EFI_UNSUPPORTED);
    else if (mScenario==5 || mScenario==6 || mScenario==10 || mScenario==13 || mScenario==18 || mScenario==19) assert(mCardInits==0 && mStatus.Board==EFI_DEVICE_ERROR);
    else if (mScenario==7) assert(mInstalls==1 && mStatus.Initialize==EFI_TIMEOUT);
    else {
      UINT8 Buffer[513];
      assert(mCardInits==1 && mInstalls==2 && mMedia.ReadOnly && mMedia.RemovableMedia);
      assert(mStatus.PadMode==1 && mStatus.IesBefore==0x80000000U && mStatus.IesAfter==0x800000fcU);
      assert(mMedia.IoAlign==1 && mMedia.LastBlock==8191);
      assert(WriteBlocks(&mBlock,1,0,512,Buffer)==EFI_WRITE_PROTECTED && mCardReads==0);
      assert(ReadBlocks(&mBlock,2,0,512,Buffer)==EFI_MEDIA_CHANGED);
      assert(ReadBlocks(&mBlock,1,0,511,Buffer)==EFI_BAD_BUFFER_SIZE);
      assert(ReadBlocks(&mBlock,1,0,512,NULL)==EFI_INVALID_PARAMETER);
      assert(ReadBlocks(&mBlock,1,0,0,NULL)==EFI_SUCCESS && mCardReads==0);
      assert(ReadBlocks(&mBlock,1,8192,512,Buffer)==EFI_INVALID_PARAMETER);
      assert(ReadBlocks(&mBlock,1,0,512,Buffer+1)==(mScenario==9 ? EFI_CRC_ERROR : EFI_SUCCESS));
      if(mScenario==9) assert(ReadBlocks(&mBlock,1,0,512,Buffer)==EFI_NO_MEDIA);
      assert(Reset(&mBlock,FALSE)==EFI_SUCCESS && mMedia.MediaId==2 && mMedia.MediaPresent);
      assert(mTpl==0);
    }
    if (mCardInits==0) assert(mStatus.Cmd8FirstStatus==EFI_NOT_STARTED);
    if (mScenario==10) assert(mStatus.PadRestoreStatus==EFI_SUCCESS && mSmt==0x80000001U && mPupd==0xdeadffffU && mR0==0xcafe00ffU && mR1==0x2300U);
    if (mScenario==13) assert(mStatus.PadRestoreStatus==EFI_DEVICE_ERROR);
    if(mScenario==14 || mScenario==18 || mScenario==19) assert(mPowerQueries==1);
    else assert(mPowerQueries==0);
    if(mScenario==14 || mScenario==18) assert(mI2cGate==(1U<<18));
    if(mScenario==15 || mScenario==16) assert(mStatus.Power.Status==EFI_UNSUPPORTED);
    if(mScenario==17) assert(mStatus.Power.Status==EFI_DEVICE_ERROR && mI2cGate==(1U<<18));
  }
  puts("SD binding: missing/untrusted HOB and mux rejection, clock readback faults, read-only/media API and recovery passed synthetic host tests; physical board untested.");
  return 0;
}
