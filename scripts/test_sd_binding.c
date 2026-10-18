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
    case 0x10000080: return mScenario == 5 ? 0x80 : 0;
    case 0x10001094: return mScenario == 6 ? Gates : 0;
    default: assert(!"unapproved register read"); return 0;
  }
}
UINT32 EFIAPI MmioWrite32 (UINTN Address, UINT32 Value)
{
  mWrites++;
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
STATIC EFI_TPL EFIAPI Raise (EFI_TPL Tpl) {assert(Tpl==TPL_NOTIFY && mTpl==0);mTpl++;return TPL_APPLICATION;}
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
  for (mScenario=0;mScenario<10;mScenario++) {
    memset(&mCard,0,sizeof(mCard)); memset(&mStatus,0,sizeof(mStatus));
    memset(&mMedia,0,sizeof(mMedia)); memset(&mHob,0,sizeof(mHob));
    mHob.Header.Header.HobLength=sizeof(mHob);
    mHob.Info.Signature=OPPO_BOOT_INFO_SIGNATURE;
    mHob.Info.Revision=OPPO_BOOT_INFO_REVISION;
    mHob.Info.Flags=mScenario==2 ? 0 : OPPO_BOOT_SD_PROFILE_VALIDATED;
    mReads=mWrites=mCardInits=mCardReads=mInstalls=mTpl=0;
    assert(OppoSdEntry(NULL,NULL)==EFI_SUCCESS);
    if (mScenario==1 || mScenario==2) assert(mReads==0 && mWrites==0 && mCardInits==0);
    else if (mScenario==3) assert(mWrites==0 && mCardInits==0 && mStatus.Board==EFI_NOT_READY);
    else if (mScenario==5 || mScenario==6) assert(mCardInits==0 && mStatus.Board==EFI_DEVICE_ERROR);
    else if (mScenario==7) assert(mInstalls==1 && mStatus.Initialize==EFI_TIMEOUT);
    else {
      UINT8 Buffer[513];
      assert(mCardInits==1 && mInstalls==2 && mMedia.ReadOnly && mMedia.RemovableMedia);
      assert(mStatus.PadMode==(mScenario==4 ? 2U : 1U));
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
  }
  puts("SD binding: missing/untrusted HOB and mux rejection, clock readback faults, read-only/media API and recovery passed synthetic host tests; physical board untested.");
  return 0;
}
