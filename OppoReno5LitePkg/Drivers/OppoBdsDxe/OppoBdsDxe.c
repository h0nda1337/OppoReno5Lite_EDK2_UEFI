/** @file
  Original laboratory BDS: connect the native console and start the FV Shell.
  No disk boot or persistent boot-variable policy. First lab runs time out.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Uefi.h>
#include <Pi/PiFirmwareFile.h>
#include <Protocol/Bds.h>
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextOut.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

STATIC EFI_HANDLE mImage;
STATIC EFI_GUID mShellFile = {0x7c04a583, 0x9e3e, 0x4f1c, {0xad, 0x65, 0xe0, 0x52, 0x68, 0xd0, 0xb4, 0xd1}};

STATIC VOID EFIAPI Timeout (EFI_EVENT Event, VOID *Context)
{
  (VOID)Event; (VOID)Context;
  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
}

STATIC VOID EFIAPI Entry (EFI_BDS_ARCH_PROTOCOL *This)
{
  EFI_HANDLE *Handles, Shell;
  UINTN Count, Index;
  EFI_STATUS Status;
  EFI_EVENT ReturnTimer;
  (VOID)This;
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  if (gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &Count, &Handles) == EFI_SUCCESS) {
    for (Index = 0; Index < Count; Index++) gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
    FreePool (Handles);
  }
  if (gBS->LocateHandleBuffer (ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &Count, &Handles) == EFI_SUCCESS) {
    for (Index = 0; Index < Count; Index++) {
      EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *Out;
      gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
      if (gBS->HandleProtocol (Handles[Index], &gEfiSimpleTextOutProtocolGuid, (VOID **)&Out) == EFI_SUCCESS) {
        gST->ConsoleOutHandle = Handles[Index]; gST->ConOut = Out;
        gST->StandardErrorHandle = Handles[Index]; gST->StdErr = Out;
        Out->ClearScreen (Out);
        Out->OutputString (Out, L"OPPO CPH2205 - experimental native EDK2\r\n");
        Out->OutputString (Out, L"Laboratory timer: reset requested after 90 seconds.\r\n\r\n");
        break;
      }
    }
    FreePool (Handles);
  }
  gST->Hdr.CRC32 = 0;
  gBS->CalculateCrc32 (gST, gST->Hdr.HeaderSize, &gST->Hdr.CRC32);
  if (gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, Timeout, NULL, &ReturnTimer) == EFI_SUCCESS)
    gBS->SetTimer (ReturnTimer, TimerRelative, 90ULL * 10000000);
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiFirmwareVolume2ProtocolGuid, NULL, &Count, &Handles);
  if (Status == EFI_SUCCESS) {
    for (Index = 0; Index < Count; Index++) {
      EFI_FIRMWARE_VOLUME2_PROTOCOL *Volume;
      VOID *Image = NULL;
      UINTN Bytes = 0;
      UINT32 Authentication;
      if (gBS->HandleProtocol (Handles[Index], &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Volume) != EFI_SUCCESS)
        continue;
      Status = Volume->ReadSection (Volume, &mShellFile, EFI_SECTION_PE32, 0, &Image, &Bytes, &Authentication);
      if (Status != EFI_SUCCESS) continue;
      MEDIA_FW_VOL_FILEPATH_DEVICE_PATH File;
      EFI_DEVICE_PATH_PROTOCOL *Path;
      File.Header.Type = MEDIA_DEVICE_PATH; File.Header.SubType = MEDIA_PIWG_FW_FILE_DP;
      SetDevicePathNodeLength (&File.Header, sizeof (File)); File.FvFileName = mShellFile;
      Path = AppendDevicePathNode (DevicePathFromHandle (Handles[Index]), &File.Header);
      Status = Path == NULL ? EFI_OUT_OF_RESOURCES : gBS->LoadImage (FALSE, mImage, Path, Image, Bytes, &Shell);
      if (Path != NULL) FreePool (Path);
      FreePool (Image);
      if (Status == EFI_SUCCESS) {
        Status = gBS->StartImage (Shell, NULL, NULL);
        gBS->UnloadImage (Shell);
      }
      break;
    }
    FreePool (Handles);
  }
  if (gST->ConOut) gST->ConOut->OutputString (gST->ConOut, L"Shell returned or could not start.\r\n");
  for (;;) gBS->Stall (100000);
}

STATIC EFI_BDS_ARCH_PROTOCOL mBds = {Entry};

EFI_STATUS EFIAPI OppoBdsEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_HANDLE Handle = NULL;
  (VOID)SystemTable;
  mImage = ImageHandle;
  return gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiBdsArchProtocolGuid, &mBds, NULL);
}
