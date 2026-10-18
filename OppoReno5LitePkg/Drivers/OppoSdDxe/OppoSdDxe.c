/** @file
  Original MT6779 MSDC1 read-only laboratory binding.
  Reuses retained SD pinmux and power, sets the validated SD input/pull bank.
  An explicit power lab may initialize validated VMC/VMCH rails to3.0V.
  Does not modify pinmux or drive strength; never writes card storage.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Uefi.h>
#include <Guid/OppoBootInfo.h>
#include <Protocol/OppoSdStatus.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <OppoNativeProfile.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/PcdLib.h>
#include <Library/OppoDebugLog.h>
#include <Library/OppoMsdcLib.h>
#include <Library/UefiBootServicesTableLib.h>

STATIC OPPO_MSDC mCard;
STATIC OPPO_SD_STATUS mStatus;
STATIC EFI_BLOCK_IO_MEDIA mMedia;
STATIC struct {
  VENDOR_DEVICE_PATH Vendor;
  EFI_DEVICE_PATH_PROTOCOL End;
} mPath;

STATIC UINT32 Read32 (VOID *Context, UINT32 Offset)
{ (VOID)Context; return MmioRead32 (0x11240000 + Offset); }
STATIC VOID Write32 (VOID *Context, UINT32 Offset, UINT32 Value)
{ (VOID)Context; MmioWrite32 (0x11240000 + Offset, Value); }
STATIC VOID DelayUs (VOID *Context, UINT32 Us)
{ (VOID)Context; MicroSecondDelay (Us); }
STATIC CONST OPPO_MSDC_IO mIo = {NULL, Read32, Write32, DelayUs};

STATIC UINT32 I2cRead (VOID *Context, UINT32 Offset)
{ (VOID)Context; return Offset == 0 ? MmioRead8 (0x11016000) : MmioRead16 (0x11016000 + Offset); }
STATIC VOID I2cWrite (VOID *Context, UINT32 Offset, UINT32 Value)
{
  (VOID)Context;
  if (Offset == 0) MmioWrite8 (0x11016000, (UINT8)Value);
  else MmioWrite16 (0x11016000 + Offset, (UINT16)Value);
}
STATIC VOID InspectPower (BOOLEAN Enable)
{
  STATIC CONST OPPO_I2C_IO Io = {NULL, I2cRead, I2cWrite, DelayUs};
  OPPO_SD_POWER *P = &mStatus.Power;
  P->ClockSelect = MmioRead32 (0x100000b0);
  P->ClockGate = MmioRead32 (0x100010ac);
  P->Pinmux = MmioRead32 (0x10005330); // GPIO28/29 -> SCL5/SDA5, function1.
  // Retain the shared source/gate and all GPIO settings. Unknown selection,
  // a gated shared root or unexpected pins stops before any I2C transaction.
  if ((P->ClockSelect & 0x83U) > 2 || (P->Pinmux & 0x00ff0000U) != 0x00110000U) {
    P->Status = EFI_UNSUPPORTED; return;
  }
  if (P->ClockGate & (1U << 18)) {
    MmioWrite32 (0x100010a8, 1U << 18); // I2C5 only, no DMA/other bus gates.
    MicroSecondDelay (100);
    if (MmioRead32 (0x100010ac) & (1U << 18)) {P->Status = EFI_DEVICE_ERROR; return;}
  }
  if (Enable) OppoSdPowerEnable (&Io, P);
  else OppoSdPowerInspect (&Io, P);
  if (P->ClockGate & (1U << 18)) {
    MmioWrite32 (0x100010a4, 1U << 18);
    UINT32 Observed = MmioRead32 (0x100010ac);
    if (!(Observed & (1U << 18))) {
      P->Restore = EFI_DEVICE_ERROR;
      P->RestoreRegister = 0x100010ac; P->RestoreExpected = 1U << 18; P->RestoreObserved = Observed & (1U << 18);
    }
  }
}

STATIC VOID RecordCard (VOID)
{
  mStatus.LastCommand = mCard.LastCommand;
  mStatus.LastInterrupt = mCard.LastInterrupt;
  mStatus.Blocks = mCard.Blocks;
  mStatus.InitialIocon = mCard.InitialIocon;
  mStatus.Cmd8Attempts = mCard.Cmd8Attempts;
  mStatus.Cmd8FirstStatus = mCard.Cmd8FirstStatus;
  mStatus.BusWidth = mCard.BusWidth; mStatus.ClockHz = mCard.ClockHz;
  mStatus.MaximumClockHz = mCard.MaximumClockHz;
  mStatus.FastSamplesVerified = mCard.FastSamplesVerified;
  mStatus.ActiveIocon = MmioRead32 (0x11240004);
  mStatus.Config = MmioRead32 (0x11240000);
  mStatus.Signal = MmioRead32 (0x11240008);
  mStatus.PadTune0 = MmioRead32 (0x112400f0);
  mMedia.MediaPresent = mCard.Ready;
  mMedia.LastBlock = mCard.Blocks ? mCard.Blocks - 1 : 0;
}

STATIC EFI_STATUS PrepareBoard (VOID)
{
  UINT32 BeforePupd, BeforeR0, BeforeR1;
  // These two mux arrangements are documented by the MT6779 GPIO contract.
  // Read both; never force pins if the bootloader left a different layout.
  mStatus.GpioMisc = MmioRead32 (0x10005600);
  mStatus.GpioMode16 = MmioRead32 (0x10005400);
  mStatus.GpioMode17 = MmioRead32 (0x10005410);
  mStatus.GpioMode1 = MmioRead32 (0x10005310);
  if (!(mStatus.GpioMisc & (1U << 9)) &&
      (mStatus.GpioMode16 & 0xff000000U) == 0x11000000U &&
      (mStatus.GpioMode17 & 0xffffU) == 0x1111U) mStatus.PadMode = 1;
  else if ((mStatus.GpioMisc & (1U << 9)) &&
           (mStatus.GpioMode1 & 0xffffff00U) == 0x11111100U) {
    mStatus.PadMode = 2;
    return EFI_UNSUPPORTED; // No validated pad-bank mapping for this alternate mux.
  }
  else return EFI_NOT_READY;
  // IOCFG_RM: only the six MSDC1 pads. Preserve every unrelated GPIO field.
  // Enable receivers/Schmitt; 50 kOhm CMD/DAT pull-up and CLK pull-down.
  mStatus.IesBefore = MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x30);
  mStatus.SmtBefore = MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0xc0);
  BeforePupd = MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x60);
  BeforeR0 = MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x80);
  BeforeR1 = MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x90);
  MmioWrite32 (OPPO_NATIVE_SD_PAD_BASE + 0x30, mStatus.IesBefore | 0xfcU);
  MmioWrite32 (OPPO_NATIVE_SD_PAD_BASE + 0xc0, mStatus.SmtBefore | 0x1cU);
  MmioWrite32 (OPPO_NATIVE_SD_PAD_BASE + 0x60, (BeforePupd & ~0x3fU) | 1U);
  MmioWrite32 (OPPO_NATIVE_SD_PAD_BASE + 0x80, BeforeR0 & ~0x3fU);
  MmioWrite32 (OPPO_NATIVE_SD_PAD_BASE + 0x90, BeforeR1 | 0x3fU);
  mStatus.IesAfter = MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x30);
  mStatus.SmtAfter = MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0xc0);
  mStatus.PullDirection = MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x60);
  mStatus.PullR0 = MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x80);
  mStatus.PullR1 = MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x90);
  if ((mStatus.IesAfter & 0xfcU) != 0xfcU || (mStatus.SmtAfter & 0x1cU) != 0x1cU ||
      (mStatus.PullDirection & 0x3fU) != 1 || (mStatus.PullR0 & 0x3fU) != 0 ||
      (mStatus.PullR1 & 0x3fU) != 0x3fU) {
    MmioWrite32 (OPPO_NATIVE_SD_PAD_BASE + 0x30, mStatus.IesBefore);
    MmioWrite32 (OPPO_NATIVE_SD_PAD_BASE + 0xc0, mStatus.SmtBefore);
    MmioWrite32 (OPPO_NATIVE_SD_PAD_BASE + 0x60, BeforePupd);
    MmioWrite32 (OPPO_NATIVE_SD_PAD_BASE + 0x80, BeforeR0);
    MmioWrite32 (OPPO_NATIVE_SD_PAD_BASE + 0x90, BeforeR1);
    mStatus.PadRestoreStatus =
      MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x30) == mStatus.IesBefore &&
      MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0xc0) == mStatus.SmtBefore &&
      MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x60) == BeforePupd &&
      MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x80) == BeforeR0 &&
      MmioRead32 (OPPO_NATIVE_SD_PAD_BASE + 0x90) == BeforeR1 ? EFI_SUCCESS : EFI_DEVICE_ERROR;
    return EFI_DEVICE_ERROR;
  }
  // CLK_CFG_6: MSDC30_1 mux -> 26 MHz, its power-down bit -> enabled.
  // CLR alias modifies only mux[2:0] and gate[7]; UPDATE bit24 latches it.
  MmioWrite32 (0x10000088, 0x87);
  MmioWrite32 (0x10000004, 1U << 24);
  MicroSecondDelay (100);
  if (MmioRead32 (0x10000080) & 0x87) return EFI_DEVICE_ERROR;
  // INFRA group1 clear alias: MSDC1 AXI and source clock only.
  MmioWrite32 (0x1000108c, (1U << 4) | (1U << 16));
  MicroSecondDelay (100);
  return (MmioRead32 (0x10001094) & ((1U << 4) | (1U << 16))) ? EFI_DEVICE_ERROR : EFI_SUCCESS;
}

STATIC EFI_STATUS InitializeCard (VOID)
{
  EFI_STATUS Status = OppoMsdcInitialize (&mCard, &mIo);
  mStatus.FastSetup = EFI_NOT_STARTED;
  if (!EFI_ERROR (Status) && FeaturePcdGet (PcdNativeSdFastReadLab)) {
    mStatus.FastSetup = OppoMsdcEnableFastRead (&mCard);
    Status = mStatus.FastSetup;
  }
  mStatus.Initialize = Status; RecordCard ();
  return Status;
}

STATIC EFI_STATUS EFIAPI Reset (EFI_BLOCK_IO_PROTOCOL *This, BOOLEAN Extended)
{
  EFI_STATUS Status;
  EFI_TPL Old;
  UINT32 Identity[4];
  UINT64 Blocks;
  BOOLEAN Same;
  (VOID)This; (VOID)Extended;
  Old = gBS->RaiseTPL (TPL_CALLBACK);
  Same = mMedia.MediaPresent;
  Blocks = mCard.Blocks;
  for (UINTN I = 0; I < ARRAY_SIZE (Identity); I++) Identity[I] = mCard.Identity[I];
  Status = InitializeCard ();
  if (!EFI_ERROR (Status)) {
    if (Blocks != mCard.Blocks) Same = FALSE;
    for (UINTN I = 0; I < ARRAY_SIZE (Identity); I++)
      if (Identity[I] != mCard.Identity[I]) Same = FALSE;
    // Resetting the same available medium does not replace it. Changing its
    // ID here makes cached partition/filesystem handles reject valid reads.
    if (!Same) mMedia.MediaId++;
  }
  OppoDebugRecordSdReset (Status, (BOOLEAN)(!EFI_ERROR (Status) && !Same));
  gBS->RestoreTPL (Old);
  return Status;
}

STATIC EFI_STATUS EFIAPI ReadBlocks (EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId,
                                     EFI_LBA Lba, UINTN Bytes, VOID *Buffer)
{
  EFI_STATUS Status;
  EFI_TPL Old;
  (VOID)This;
  if (MediaId != mMedia.MediaId) Status = EFI_MEDIA_CHANGED;
  else if (!mMedia.MediaPresent) Status = EFI_NO_MEDIA;
  else if (Bytes == 0) Status = EFI_SUCCESS;
  else if (Buffer == NULL) Status = EFI_INVALID_PARAMETER;
  else if (Bytes % 512) Status = EFI_BAD_BUFFER_SIZE;
  else {
    Old = gBS->RaiseTPL (TPL_CALLBACK);
    Status = OppoMsdcRead (&mCard, Lba, Bytes / 512, Buffer);
    RecordCard ();
    gBS->RestoreTPL (Old);
  }
  OppoDebugRecordSdRead (Status, Bytes);
  return Status;
}

STATIC EFI_STATUS EFIAPI WriteBlocks (EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId,
                                      EFI_LBA Lba, UINTN Bytes, VOID *Buffer)
{
  (VOID)This; (VOID)MediaId; (VOID)Lba; (VOID)Bytes; (VOID)Buffer;
  return EFI_WRITE_PROTECTED;
}
STATIC EFI_STATUS EFIAPI Flush (EFI_BLOCK_IO_PROTOCOL *This)
{ (VOID)This; return EFI_SUCCESS; }
STATIC EFI_BLOCK_IO_PROTOCOL mBlock = {EFI_BLOCK_IO_PROTOCOL_REVISION, &mMedia,
                                      Reset, ReadBlocks, WriteBlocks, Flush};

EFI_STATUS EFIAPI OppoSdEntry (EFI_HANDLE Image, EFI_SYSTEM_TABLE *System)
{
  VOID *Hob;
  OPPO_BOOT_INFO *Info;
  EFI_HANDLE Handle = NULL;
  EFI_STATUS Status;
  (VOID)Image; (VOID)System;
  mStatus.Profile = EFI_UNSUPPORTED;
  mStatus.Board = mStatus.Initialize = mStatus.Publish = EFI_NOT_STARTED;
  mStatus.Cmd8FirstStatus = mStatus.PadRestoreStatus = EFI_NOT_STARTED;
  mStatus.FastSetup = EFI_NOT_STARTED;
  mStatus.PowerProfile = EFI_UNSUPPORTED;
  mStatus.Power.Status = mStatus.Power.Restore = EFI_NOT_STARTED;
  mStatus.Power.LastAddress = mStatus.Power.LastRegister = MAX_UINT32;
  mStatus.Power.RestoreRegister = MAX_UINT32;
  mStatus.Power.Apply = mStatus.Power.Rollback = EFI_NOT_STARTED;
  mStatus.Power.ApplyFailureRegister = MAX_UINT32;
  mStatus.LastCommand = MAX_UINT32;
  // Keep diagnostics alive even if no media is found. Returning an error after
  // this installation would unload the image and leave a dangling interface.
  Status = gBS->InstallMultipleProtocolInterfaces (&Handle, &gOppoSdStatusProtocolGuid, &mStatus, NULL);
  if (EFI_ERROR (Status)) return Status;
  Hob = GetFirstGuidHob (&gOppoBootInfoHobGuid);
  if (Hob == NULL || GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (*Info)) return EFI_SUCCESS;
  Info = GET_GUID_HOB_DATA (Hob);
  if (Info->Signature != OPPO_BOOT_INFO_SIGNATURE || Info->Revision != OPPO_BOOT_INFO_REVISION ||
      (Info->Flags & (OPPO_BOOT_SD_PROFILE_VALIDATED | OPPO_BOOT_SD_PAD_BANK_VALIDATED | OPPO_BOOT_SD_RM_ROLE_VALIDATED)) !=
      (OPPO_BOOT_SD_PROFILE_VALIDATED | OPPO_BOOT_SD_PAD_BANK_VALIDATED | OPPO_BOOT_SD_RM_ROLE_VALIDATED)) return EFI_SUCCESS;
  mStatus.Profile = EFI_SUCCESS;
  if (FeaturePcdGet (PcdNativeSdPowerLab) &&
      (Info->Flags & (OPPO_BOOT_SD_POWER_PROFILE_VALIDATED | OPPO_BOOT_SD_VOLTAGE_PROFILE_VALIDATED)) !=
      (OPPO_BOOT_SD_POWER_PROFILE_VALIDATED | OPPO_BOOT_SD_VOLTAGE_PROFILE_VALIDATED)) {
    mStatus.Board = mStatus.Power.Apply = EFI_UNSUPPORTED; return EFI_SUCCESS;
  }
  mStatus.Board = PrepareBoard ();
  if (EFI_ERROR (mStatus.Board)) return EFI_SUCCESS;
  if (Info->Flags & OPPO_BOOT_SD_POWER_PROFILE_VALIDATED) {
    mStatus.PowerProfile = EFI_SUCCESS;
    InspectPower (FeaturePcdGet (PcdNativeSdPowerLab));
    if (mStatus.Power.Restore == EFI_DEVICE_ERROR) {mStatus.Board = EFI_DEVICE_ERROR; return EFI_SUCCESS;}
    if (FeaturePcdGet (PcdNativeSdPowerLab) &&
        (mStatus.Power.Apply != EFI_SUCCESS || mStatus.Power.Status != EFI_SUCCESS ||
         mStatus.Power.Restore != EFI_SUCCESS)) {mStatus.Board = EFI_DEVICE_ERROR; return EFI_SUCCESS;}
  }
  mStatus.Initialize = InitializeCard ();
  if (EFI_ERROR (mStatus.Initialize)) return EFI_SUCCESS;
  mMedia.MediaId = 1; mMedia.RemovableMedia = TRUE; mMedia.ReadOnly = TRUE;
  mMedia.BlockSize = 512; mMedia.IoAlign = 1;
  mPath.Vendor.Header.Type = HARDWARE_DEVICE_PATH;
  mPath.Vendor.Header.SubType = HW_VENDOR_DP;
  mPath.Vendor.Header.Length[0] = sizeof (VENDOR_DEVICE_PATH);
  mPath.Vendor.Guid = gOppoSdDeviceGuid;
  mPath.End.Type = END_DEVICE_PATH_TYPE; mPath.End.SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
  mPath.End.Length[0] = sizeof (EFI_DEVICE_PATH_PROTOCOL);
  mStatus.Publish = gBS->InstallMultipleProtocolInterfaces (&Handle,
    &gEfiBlockIoProtocolGuid, &mBlock, &gEfiDevicePathProtocolGuid, &mPath, NULL);
  return EFI_SUCCESS;
}
