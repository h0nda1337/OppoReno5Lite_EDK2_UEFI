/* SPDX-License-Identifier: BSD-2-Clause */
/* Original SD-only discovery. Never searches internal storage or changes files. */
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DevicePath.h>
#include <Protocol/OppoSdStatus.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>

STATIC BOOLEAN IsOurSd (EFI_HANDLE Handle)
{
  EFI_DEVICE_PATH_PROTOCOL *Path;
  if (gBS->HandleProtocol (Handle, &gEfiDevicePathProtocolGuid, (VOID **)&Path) != EFI_SUCCESS)
    return FALSE;
  // The SD vendor node must be the root; no arbitrary device-path traversal.
  return Path != NULL && Path->Type == HARDWARE_DEVICE_PATH && Path->SubType == HW_VENDOR_DP &&
    DevicePathNodeLength (Path) == sizeof (VENDOR_DEVICE_PATH) &&
    CompareGuid (&((VENDOR_DEVICE_PATH *)Path)->Guid, &gOppoSdDeviceGuid);
}

STATIC UINT32 Le (CONST UINT8 *P, UINTN Size)
{
  UINT32 Value = 0;
  for (UINTN I = 0; I < Size; I++) Value |= (UINT32)P[I] << (8 * I);
  return Value;
}

VOID OppoSdReadLab (VOID)
{
  OPPO_SD_STATUS *Sd;
  EFI_HANDLE *Handles;
  UINTN Count, Bytes, Pe;
  EFI_STATUS Status;
  BOOLEAN Found = FALSE;
  Status = gBS->LocateProtocol (&gOppoSdStatusProtocolGuid, NULL, (VOID **)&Sd);
  DEBUG ((DEBUG_INFO, "SD read-only lab; status protocol: %r\n", Status));
  if (EFI_ERROR (Status)) return;
  DEBUG ((DEBUG_INFO, "SD profile=%r board=%r init=%r publish=%r\n",
          Sd->Profile, Sd->Board, Sd->Initialize, Sd->Publish));
  DEBUG ((DEBUG_INFO, "SD pads=%u misc=%08x mode16=%08x mode17=%08x mode1=%08x\n",
          Sd->PadMode, Sd->GpioMisc, Sd->GpioMode16, Sd->GpioMode17, Sd->GpioMode1));
  DEBUG ((DEBUG_INFO, "SD sectors=%Lu last CMD=%u INT=%08x\n",
          Sd->Blocks, Sd->LastCommand, Sd->LastInterrupt));
  DEBUG ((DEBUG_INFO, "SD IOCON before=%08x active=%08x CFG=%08x PS=%08x\n",
          Sd->InitialIocon, Sd->ActiveIocon, Sd->Config, Sd->Signal));
  DEBUG ((DEBUG_INFO, "SD CMD8 first=%r attempts=%u PAD_TUNE0=%08x\n",
          Sd->Cmd8FirstStatus, Sd->Cmd8Attempts, Sd->PadTune0));
  DEBUG ((DEBUG_INFO, "SD IES before=%08x after=%08x SMT before=%08x after=%08x\n",
          Sd->IesBefore, Sd->IesAfter, Sd->SmtBefore, Sd->SmtAfter));
  DEBUG ((DEBUG_INFO, "SD pulls PUPD=%08x R0=%08x R1=%08x\n",
          Sd->PullDirection, Sd->PullR0, Sd->PullR1));
  if (EFI_ERROR (Sd->Publish)) return;
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count, &Handles);
  DEBUG ((DEBUG_INFO, "SD filesystem discovery: %r\n", Status));
  if (EFI_ERROR (Status)) return;
  for (UINTN I = 0; I < Count; I++) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
    EFI_FILE_PROTOCOL *Root = NULL, *File = NULL;
    UINT8 *Header;
    if (!IsOurSd (Handles[I])) continue;
    Found = TRUE;
    Status = gBS->HandleProtocol (Handles[I], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
    if (!EFI_ERROR (Status)) Status = Fs->OpenVolume (Fs, &Root);
    if (EFI_ERROR (Status)) { DEBUG ((DEBUG_INFO, "SD OpenVolume: %r\n", Status)); continue; }
    Status = Root->Open (Root, &File, L"\\EFI\\BOOT\\BOOTAA64.EFI", EFI_FILE_MODE_READ, 0);
    DEBUG ((DEBUG_INFO, "SD open BOOTAA64.EFI: %r\n", Status));
    if (!EFI_ERROR (Status)) {
      Header = AllocatePool (4096);
      Bytes = 4096;
      Status = Header == NULL ? EFI_OUT_OF_RESOURCES : File->Read (File, &Bytes, Header);
      DEBUG ((DEBUG_INFO, "SD file prefix: %r bytes=%u\n", Status, (UINT32)Bytes));
      if (!EFI_ERROR (Status) && Bytes >= 64 && Header[0] == 'M' && Header[1] == 'Z') {
        Pe = Le (Header + 0x3c, 4);
        if (Pe <= Bytes && Bytes - Pe >= 94 && Le (Header + Pe, 4) == 0x4550 &&
            Le (Header + Pe + 4, 2) == 0xaa64 && Le (Header + Pe + 24, 2) == 0x20b &&
            Le (Header + Pe + 92, 2) == 10)
          DEBUG ((DEBUG_INFO, "SD ARM64 UEFI header verified from physical media\n"));
        else DEBUG ((DEBUG_INFO, "SD file header unsupported or outside prefix\n"));
      }
      if (Header != NULL) FreePool (Header);
      File->Close (File);
    }
    Root->Close (Root);
  }
  FreePool (Handles);
  if (!Found) DEBUG ((DEBUG_INFO, "SD has no readable FAT filesystem handle\n"));
  DEBUG ((DEBUG_INFO, "SD read lab finished; Windows not launched in this variant\n"));
}
