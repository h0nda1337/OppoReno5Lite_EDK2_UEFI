/* SPDX-License-Identifier: BSD-2-Clause */
/* Invoke the actual original GOP driver with host-only UEFI service fixtures. */
#include <PiDxe.h>
#include <Guid/OppoBootInfo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/HobLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

EFI_GUID gOppoBootInfoHobGuid = {.Data1 = 1};
EFI_GUID gOppoGopDeviceGuid = {.Data1 = 2};
EFI_GUID gEfiDevicePathProtocolGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;
EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
STATIC struct { EFI_HOB_GUID_TYPE Header; OPPO_BOOT_INFO Info; } mHob;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mInstalled;
STATIC UINTN mInstallCount;
STATIC EFI_BOOT_SERVICES mServices;
EFI_BOOT_SERVICES *gBS = &mServices;

VOID *EFIAPI AllocateCopyPool(UINTN Size, CONST VOID *Buffer)
{
  VOID *copy = malloc(Size);
  if (copy) memcpy(copy, Buffer, Size);
  return copy;
}
VOID EFIAPI MemoryFence(VOID) { __sync_synchronize(); }
VOID *EFIAPI GetFirstGuidHob(CONST EFI_GUID *Guid)
{
  return Guid == &gOppoBootInfoHobGuid ? &mHob : NULL;
}
STATIC EFI_STATUS EFIAPI Install(EFI_HANDLE *Handle, ...)
{
  va_list args;
  EFI_GUID *guid;
  va_start(args, Handle);
  while ((guid = va_arg(args, EFI_GUID *)) != NULL) {
    VOID *interface = va_arg(args, VOID *);
    if (guid == &gEfiGraphicsOutputProtocolGuid) mInstalled = interface;
  }
  va_end(args);
  *Handle = (VOID *)(UINTN)1; mInstallCount++;
  return EFI_SUCCESS;
}
EFI_STATUS EFIAPI OppoGopEntry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

int main(void)
{
  UINT32 guarded[64], before[64];
  for (UINTN i = 0; i < 64; i++) guarded[i] = 0x12345678;
  mServices.InstallMultipleProtocolInterfaces = Install;
  mHob.Header.Header.HobType = EFI_HOB_TYPE_GUID_EXTENSION;
  mHob.Header.Header.HobLength = sizeof mHob;
  mHob.Info.Signature = OPPO_BOOT_INFO_SIGNATURE;
  mHob.Info.Revision = OPPO_BOOT_INFO_REVISION;
  mHob.Info.Flags = OPPO_BOOT_MEMORY_READY | OPPO_BOOT_EXPECTED_DISPLAY_PROFILE;
  mHob.Info.FramebufferBase = (UINTN)(guarded + 2); mHob.Info.FramebufferBytes = 240;
  mHob.Info.Width = 8; mHob.Info.Height = 6; mHob.Info.Stride = 40;
  mHob.Info.RedShift = 0; mHob.Info.BlueShift = 16; mHob.Info.OpaqueAlpha = TRUE;
  mHob.Info.Signature++;
  assert(OppoGopEntry(NULL, NULL) == EFI_COMPROMISED_DATA && mInstallCount == 0);
  mHob.Info.Signature--;
  mHob.Info.FramebufferBytes = 239;
  assert(OppoGopEntry(NULL, NULL) == EFI_UNSUPPORTED && mInstallCount == 0);
  mHob.Info.FramebufferBytes = 240;
  assert(OppoGopEntry(NULL, NULL) == EFI_SUCCESS && mInstallCount == 1 && mInstalled);
  assert(mInstalled->Mode->MaxMode == 1 && mInstalled->Mode->Info->PixelsPerScanLine == 10);
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode;
  UINTN mode_size;
  assert(mInstalled->QueryMode(mInstalled, 0, &mode_size, &mode) == EFI_SUCCESS);
  assert(mode_size == sizeof *mode && mode->PixelFormat == PixelRedGreenBlueReserved8BitPerColor);
  free(mode);
  assert(mInstalled->SetMode(mInstalled, 1) == EFI_UNSUPPORTED);
  assert(mInstalled->SetMode(mInstalled, 0) == EFI_SUCCESS);
  for (UINTN i = 0; i < 64; i++) {
    BOOLEAN visible = i >= 2 && i < 62 && (i - 2) % 10 < 8;
    assert(guarded[i] == (visible ? 0xff000000 : 0x12345678));
  }
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL color = {0x56, 0x34, 0x12, 0}, output[6];
  assert(mInstalled->Blt(mInstalled, &color, EfiBltVideoFill, 0, 0, 1, 1, 2, 3, 0) == EFI_SUCCESS);
  assert(guarded[13] == 0xff563412);
  assert(mInstalled->Blt(mInstalled, output, EfiBltVideoToBltBuffer, 1, 1, 0, 0, 2, 3, 0) == EFI_SUCCESS);
  assert(output[5].Red == 0x12 && output[5].Blue == 0x56 && output[5].Reserved == 0);
  assert(mInstalled->Blt(mInstalled, output, EfiBltBufferToVideo, 0, 0, 4, 2, 2, 3, 0) == EFI_SUCCESS);
  assert(guarded[26] == 0xff563412);
  assert(mInstalled->Blt(mInstalled, NULL, EfiBltVideoToVideo, 4, 2, 5, 3, 2, 3, 0) == EFI_SUCCESS);
  memcpy(before, guarded, sizeof before);
  assert(mInstalled->Blt(mInstalled, output, EfiBltBufferToVideo, MAX_UINTN, 0, 0, 0, 1, 1, 0) == EFI_INVALID_PARAMETER);
  assert(mInstalled->Blt(mInstalled, output, EfiBltVideoToBltBuffer, 0, 0, 0, MAX_UINTN, 1, 1, 0) == EFI_INVALID_PARAMETER);
  assert(mInstalled->Blt(mInstalled, output, EfiBltVideoFill, 0, 0, 8, 5, 1, 1, 0) == EFI_INVALID_PARAMETER);
  assert(memcmp(before, guarded, sizeof before) == 0);
  puts("Original GOP entry and all four protocol BLTs passed with synthetic RAM and host sanitizers; no physical display test.");
  return 0;
}
