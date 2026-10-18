/* SPDX-License-Identifier: BSD-2-Clause */
/* Original SD-only discovery. Never searches internal storage or changes files. */
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DevicePath.h>
#include <Protocol/OppoSdStatus.h>
#include <Guid/EventGroup.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PcdLib.h>
#include <Library/OppoDebugLog.h>
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

STATIC VOID EFIAPI BootPhaseNotify (EFI_EVENT Event, VOID *Context)
{ (VOID)Event; (VOID)Context; }

STATIC EFI_STATUS SignalBootPhase (EFI_GUID *Group)
{
  EFI_EVENT Event;
  EFI_STATUS Status, Cleanup;
  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, BootPhaseNotify,
                              NULL, Group, &Event);
  if (EFI_ERROR (Status)) return Status;
  Status = gBS->SignalEvent (Event);
  Cleanup = gBS->CloseEvent (Event);
  return EFI_ERROR (Status) ? Status : Cleanup;
}

STATIC VOID SnapshotMemory (OPPO_BOOT_FLOW *Flow)
{
  UINTN Bytes = 0, Key, Step = 0;
  UINT32 Version;
  EFI_MEMORY_DESCRIPTOR *Map;
  Flow->ConventionalBytes = 0;
  Flow->MapStatus = gBS->GetMemoryMap (&Bytes, NULL, &Key, &Step, &Version);
  if (Flow->MapStatus != EFI_BUFFER_TOO_SMALL) {
    if (!EFI_ERROR (Flow->MapStatus)) Flow->MapStatus = EFI_BAD_BUFFER_SIZE;
    return;
  }
  if (Step < sizeof (*Map) || Step > 65536 / 8 || Bytes == 0 || Bytes > 65536 - Step * 8) {
    Flow->MapStatus = EFI_BAD_BUFFER_SIZE; return;
  }
  UINTN Capacity = Bytes + Step * 8;
  Map = AllocatePool (Capacity);
  if (Map == NULL) {Flow->MapStatus = EFI_OUT_OF_RESOURCES; return;}
  Bytes = Capacity;
  Flow->MapStatus = gBS->GetMemoryMap (&Bytes, Map, &Key, &Step, &Version);
  if (!EFI_ERROR (Flow->MapStatus)) {
    if (Version != EFI_MEMORY_DESCRIPTOR_VERSION || Step < sizeof (*Map) ||
        Bytes > Capacity || Bytes == 0 || Bytes % Step != 0) Flow->MapStatus = EFI_BAD_BUFFER_SIZE;
    else for (UINTN Offset = 0; Offset < Bytes; Offset += Step) {
      EFI_MEMORY_DESCRIPTOR Item;
      CopyMem (&Item, (UINT8 *)Map + Offset, sizeof (Item));
      if (Item.Type != EfiConventionalMemory) continue;
      if (Item.NumberOfPages > (MAX_UINT64 - Flow->ConventionalBytes) / EFI_PAGE_SIZE) {
        Flow->MapStatus = EFI_BAD_BUFFER_SIZE; break;
      }
      Flow->ConventionalBytes += Item.NumberOfPages * EFI_PAGE_SIZE;
    }
  }
  if (EFI_ERROR (Flow->MapStatus)) Flow->ConventionalBytes = 0;
  FreePool (Map);
  // This is an observation while the map buffer was allocated. It is not used
  // for allocation, address selection or ExitBootServices and its key is unused.
}

STATIC VOID BootFromSd (EFI_HANDLE Parent, EFI_HANDLE Medium)
{
  EFI_DEVICE_PATH_PROTOCOL *Path;
  EFI_HANDLE Child = NULL;
  EFI_STATUS Status;
  OPPO_BOOT_FLOW Flow = {OPPO_BOOT_BEFORE_LOAD, EFI_NOT_STARTED, EFI_NOT_STARTED, EFI_NOT_STARTED, 0};
  if (Parent == NULL) {
    DEBUG ((DEBUG_ERROR, "SD EFI boot missing parent image handle\n")); return;
  }
  Path = FileDevicePath (Medium, L"\\EFI\\BOOT\\BOOTAA64.EFI");
  if (Path == NULL) {
    DEBUG ((DEBUG_ERROR, "SD EFI boot device path allocation failed\n")); return;
  }
  // BDS has connected the firmware controllers. Complete the DXE phase before
  // loading an external application; SecurityStub otherwise defers it. Keep
  // LoadImage and all registered security handlers in the normal loading path.
  Status = SignalBootPhase (&gEfiEndOfDxeEventGroupGuid);
  DEBUG ((DEBUG_INFO, "SD EFI EndOfDxe: %r\n", Status));
  if (!EFI_ERROR (Status)) {
    Status = SignalBootPhase (&gEfiEventReadyToBootGuid);
    DEBUG ((DEBUG_INFO, "SD EFI ReadyToBoot: %r\n", Status));
  }
  if (EFI_ERROR (Status)) {FreePool (Path); return;}
  SnapshotMemory (&Flow);
  OppoDebugRecordBootFlow (&Flow);
  // Let the firmware load from the SD filesystem so its security policy and
  // the loaded image's device/path identity remain effective. No memory blob.
  Status = gBS->LoadImage (FALSE, Parent, Path, NULL, 0, &Child);
  FreePool (Path);
  Flow.Stage = OPPO_BOOT_LOAD_RETURNED; Flow.LoadStatus = Status;
  OppoDebugRecordBootFlow (&Flow);
  DEBUG ((DEBUG_INFO, "SD EFI LoadImage returned: %r\n", Status));
  if (EFI_ERROR (Status)) {
    // This particular status can leave a loaded image that must not be started.
    if (Status == EFI_SECURITY_VIOLATION && Child != NULL) {
      EFI_STATUS Cleanup = gBS->UnloadImage (Child);
      DEBUG ((DEBUG_INFO, "SD EFI rejected image cleanup: %r\n", Cleanup));
    }
    return;
  }
  if (Child == NULL) {
    DEBUG ((DEBUG_ERROR, "SD EFI loader returned no image handle\n")); return;
  }
  DEBUG ((DEBUG_INFO, "SD EFI StartImage attempt from verified ARM64 prefix\n"));
  Flow.Stage = OPPO_BOOT_START_ACTIVE;
  SnapshotMemory (&Flow);
  OppoDebugRecordBootFlow (&Flow);
  Status = gBS->StartImage (Child, NULL, NULL);
  Flow.Stage = OPPO_BOOT_START_RETURNED; Flow.StartStatus = Status;
  OppoDebugRecordBootFlow (&Flow);
  // UEFI applications are unloaded when they return. Do not unload twice.
  DEBUG ((DEBUG_INFO, "SD EFI StartImage returned: %r\n", Status));
}

VOID OppoSdReadLab (EFI_HANDLE Parent)
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
  DEBUG ((DEBUG_INFO, "SD pad rollback=%r\n", Sd->PadRestoreStatus));
  DEBUG ((DEBUG_INFO, "SD power profile=%r query=%r restore=%r valid=%02x\n",
    Sd->PowerProfile, Sd->Power.Status, Sd->Power.Restore, Sd->Power.Valid));
  DEBUG ((DEBUG_INFO, "SD I2C clock=%08x gate=%08x pins=%08x INT=%04x addr=%02x reg=%02x\n",
    Sd->Power.ClockSelect, Sd->Power.ClockGate, Sd->Power.Pinmux,
    Sd->Power.Interrupt, Sd->Power.LastAddress, Sd->Power.LastRegister));
  DEBUG ((DEBUG_INFO, "SD PMIC ID=%02x VMC=%02x/%02x/%02x VMCH=%02x/%02x/%02x\n",
    Sd->Power.ChipId, Sd->Power.VmcControl1, Sd->Power.VmcControl2, Sd->Power.VmcVoltage,
    Sd->Power.VmchControl1, Sd->Power.VmchControl2, Sd->Power.VmchVoltage));
  DEBUG ((DEBUG_INFO, "SD I2C restore reg=%08x expected=%04x observed=%04x\n",
    Sd->Power.RestoreRegister, Sd->Power.RestoreExpected, Sd->Power.RestoreObserved));
  DEBUG ((DEBUG_INFO, "SD power apply=%r rollback=%r failure reg=%02x\n",
    Sd->Power.Apply, Sd->Power.Rollback, Sd->Power.ApplyFailureRegister));
  DEBUG ((DEBUG_INFO, "SD active VMC=%02x/%02x VMCH=%02x/%02x\n",
    Sd->Power.ActiveVmcControl, Sd->Power.ActiveVmcVoltage,
    Sd->Power.ActiveVmchControl, Sd->Power.ActiveVmchVoltage));
  if (EFI_ERROR (Sd->Publish)) return;
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count, &Handles);
  DEBUG ((DEBUG_INFO, "SD filesystem discovery: %r\n", Status));
  if (EFI_ERROR (Status)) return;
  for (UINTN I = 0; I < Count; I++) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
    EFI_FILE_PROTOCOL *Root = NULL, *File = NULL;
    UINT8 *Header;
    BOOLEAN Verified = FALSE;
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
            Le (Header + Pe + 92, 2) == 10) {
          Verified = TRUE;
          DEBUG ((DEBUG_INFO, "SD ARM64 UEFI header verified from physical media\n"));
        }
        else DEBUG ((DEBUG_INFO, "SD file header unsupported or outside prefix\n"));
      }
      if (Header != NULL) FreePool (Header);
      File->Close (File);
    }
    Root->Close (Root);
    if (Verified && FeaturePcdGet (PcdNativeSdBootLab)) {
      BootFromSd (Parent, Handles[I]);
      break; // At most one boot attempt, from this driver's SD device only.
    }
  }
  FreePool (Handles);
  if (!Found) DEBUG ((DEBUG_INFO, "SD has no readable FAT filesystem handle\n"));
  if (!FeaturePcdGet (PcdNativeSdBootLab))
    DEBUG ((DEBUG_INFO, "SD read lab finished; Windows not launched in this variant\n"));
}
