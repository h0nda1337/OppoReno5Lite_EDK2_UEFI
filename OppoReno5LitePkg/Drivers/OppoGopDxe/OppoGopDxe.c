/** @file
  Original single-mode GOP consuming the platform's validated boot-info HOB.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <PiDxe.h>
#include <Guid/OppoBootInfo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/HobLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/OppoFrameBufferLib.h>

STATIC OPPO_FB_CONTEXT mFramebuffer;
STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION mInfo;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mMode;
STATIC EFI_HANDLE mHandle;
STATIC struct { VENDOR_DEVICE_PATH Vendor; EFI_DEVICE_PATH_PROTOCOL End; } mPath;

STATIC EFI_STATUS EFIAPI QueryMode (EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 Mode,
                                    UINTN *Size, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info)
{
  if (This == NULL || Mode != 0 || Size == NULL || Info == NULL) return EFI_INVALID_PARAMETER;
  *Info = AllocateCopyPool (sizeof (mInfo), &mInfo);
  if (*Info == NULL) return EFI_OUT_OF_RESOURCES;
  *Size = sizeof (mInfo);
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI SetMode (EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 Mode)
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = {0, 0, 0, 0};
  if (This == NULL) return EFI_INVALID_PARAMETER;
  if (Mode != 0) return EFI_UNSUPPORTED;
  EFI_STATUS Status = OppoFbBlt (&mFramebuffer, &Black, sizeof (Black), EfiBltVideoFill,
                                 0, 0, 0, 0, mInfo.HorizontalResolution, mInfo.VerticalResolution, 0);
  if (Status == EFI_SUCCESS) MemoryFence ();
  return Status;
}

STATIC EFI_STATUS EFIAPI Blt (EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buffer, EFI_GRAPHICS_OUTPUT_BLT_OPERATION Operation,
  UINTN SourceX, UINTN SourceY, UINTN DestinationX, UINTN DestinationY,
  UINTN Width, UINTN Height, UINTN Delta)
{
  UINTN Bytes = 0;
  if (This == NULL || Width == 0 || Height == 0 ||
      (UINTN)Operation >= EfiGraphicsOutputBltOperationMax) return EFI_INVALID_PARAMETER;
  if (Operation == EfiBltVideoFill) Bytes = sizeof (*Buffer);
  else if (Operation != EfiBltVideoToVideo) {
    UINTN X = Operation == EfiBltBufferToVideo ? SourceX : DestinationX;
    UINTN Y = Operation == EfiBltBufferToVideo ? SourceY : DestinationY;
    UINTN LastRow, RowEnd;
    if (X > MAX_UINTN / 4 || Width > MAX_UINTN / 4 - X || Height - 1 > MAX_UINTN - Y)
      return EFI_INVALID_PARAMETER;
    RowEnd = (X + Width) * 4; LastRow = Y + Height - 1;
    if (Delta == 0) Delta = Width * 4;
    if (Delta < RowEnd || (LastRow != 0 && Delta > (MAX_UINTN - RowEnd) / LastRow))
      return EFI_INVALID_PARAMETER;
    // GOP callers own their buffer allocation; the interface supplies no size.
    Bytes = LastRow * Delta + RowEnd;
  }
  EFI_STATUS Status = OppoFbBlt (&mFramebuffer, Buffer, Bytes, Operation, SourceX, SourceY,
                                 DestinationX, DestinationY, Width, Height, Delta);
  if (Status == EFI_SUCCESS) MemoryFence ();
  return Status;
}

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL mGop = {QueryMode, SetMode, Blt, &mMode};

EFI_STATUS EFIAPI OppoGopEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  VOID *Hob = GetFirstGuidHob (&gOppoBootInfoHobGuid);
  CONST OPPO_BOOT_INFO *Boot;
  OPPO_FB_LAYOUT Layout;
  (VOID)ImageHandle; (VOID)SystemTable;
  if (Hob == NULL || GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (*Boot)) return EFI_NOT_FOUND;
  Boot = GET_GUID_HOB_DATA (Hob);
  if (Boot->Signature != OPPO_BOOT_INFO_SIGNATURE || Boot->Revision != OPPO_BOOT_INFO_REVISION ||
      (Boot->Flags & 3) != 3 || Boot->FramebufferBase == 0 ||
      Boot->FramebufferBase > MAX_UINTN || Boot->FramebufferBytes > MAX_UINTN)
    return EFI_COMPROMISED_DATA;
  Layout = (OPPO_FB_LAYOUT){Boot->Width, Boot->Height, Boot->Stride,
                            Boot->RedShift, Boot->BlueShift, Boot->OpaqueAlpha};
  if (!OppoFbInitialize (&mFramebuffer, (VOID *)(UINTN)Boot->FramebufferBase,
                         (UINTN)Boot->FramebufferBytes, &Layout)) return EFI_UNSUPPORTED;
  mInfo.Version = 0; mInfo.HorizontalResolution = Boot->Width; mInfo.VerticalResolution = Boot->Height;
  mInfo.PixelFormat = Boot->RedShift == 0 ? PixelRedGreenBlueReserved8BitPerColor : PixelBlueGreenRedReserved8BitPerColor;
  mInfo.PixelsPerScanLine = Boot->Stride / 4;
  mMode.MaxMode = 1; mMode.Mode = 0; mMode.Info = &mInfo; mMode.SizeOfInfo = sizeof (mInfo);
  mMode.FrameBufferBase = Boot->FramebufferBase;
  mMode.FrameBufferSize = (UINTN)Boot->Stride * Boot->Height;
  mPath.Vendor.Header.Type = HARDWARE_DEVICE_PATH;
  mPath.Vendor.Header.SubType = HW_VENDOR_DP;
  mPath.Vendor.Header.Length[0] = sizeof (mPath.Vendor); mPath.Vendor.Header.Length[1] = 0;
  mPath.Vendor.Guid = gOppoGopDeviceGuid;
  mPath.End.Type = END_DEVICE_PATH_TYPE; mPath.End.SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
  mPath.End.Length[0] = sizeof (mPath.End); mPath.End.Length[1] = 0;
  return gBS->InstallMultipleProtocolInterfaces (&mHandle, &gEfiDevicePathProtocolGuid, &mPath,
                                                 &gEfiGraphicsOutputProtocolGuid, &mGop, NULL);
}
