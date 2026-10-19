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
#include <Protocol/HiiDatabase.h>
#include <Protocol/HiiFont.h>
#include <Protocol/HiiString.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/ArmLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

STATIC EFI_HANDLE mImage;
VOID OppoSdReadLab (EFI_HANDLE Parent);
VOID OppoAcpiProbeLab (VOID);
EFI_STATUS OppoAcpiPrepareForOs (VOID);
STATIC UINT32 DeadlineSeconds (VOID)
{ return FeaturePcdGet (PcdNativeExtraMemoryLab) ? 1800U : 90U; }

STATIC BOOLEAN LabIdentityValid (VOID)
{
  if (FeaturePcdGet (PcdNativeSdAcpiBootLab) &&
      (!FeaturePcdGet (PcdNativeSdReadLab) || !FeaturePcdGet (PcdNativeSdBootLab) ||
       !FeaturePcdGet (PcdNativeSdFastReadLab) || !FeaturePcdGet (PcdNativeExtraMemoryLab) ||
       FeaturePcdGet (PcdNativeAcpiProfileLab) || FeaturePcdGet (PcdNativeAcpiInstallLab) ||
       FeaturePcdGet (PcdNativeCpuProfileLab))) return FALSE;
  if (FeaturePcdGet (PcdNativeAcpiInstallLab) && !FeaturePcdGet (PcdNativeAcpiProfileLab)) return FALSE;
  if (FeaturePcdGet (PcdNativeCpuProfileLab) &&
      (!FeaturePcdGet (PcdNativeAcpiProfileLab) || FeaturePcdGet (PcdNativeAcpiInstallLab))) return FALSE;
  if (FeaturePcdGet (PcdNativeAcpiProfileLab) &&
      (FeaturePcdGet (PcdNativeSdReadLab) || FeaturePcdGet (PcdNativeSdBootLab) ||
       FeaturePcdGet (PcdNativeSdFastReadLab) || FeaturePcdGet (PcdNativeExtraMemoryLab))) return FALSE;
  if (FeaturePcdGet (PcdNativeExtraMemoryLab)) {
    STATIC CONST volatile CHAR8 Identity[] = "OPPO_NATIVE_DXE_SHELL_LAB_1800S_V1";
    return (BOOLEAN)(Identity[0] == 'O');
  } else {
    STATIC CONST volatile CHAR8 Identity[] = "OPPO_NATIVE_DXE_SHELL_LAB_90S_V1";
    return (BOOLEAN)(Identity[0] == 'O');
  }
}
STATIC EFI_GUID mShellFile = {0x7c04a583, 0x9e3e, 0x4f1c, {0xad, 0x65, 0xe0, 0x52, 0x68, 0xd0, 0xb4, 0xd1}};
STATIC BOOLEAN mCounterDeadlineReady;
STATIC UINT64 mCounterStart, mCounterTicks;
STATIC EFI_STATUS mEventStatus, mTimerStatus;

typedef struct {
  INT32 InitialMode, SelectedMode;
  UINTN Columns, Rows;
  EFI_STATUS SetStatus, ClearStatus;
} OPPO_TEXT_SETUP;

// Activate a mode even when the driver advertises a preferred index: SetMode
// also allocates the graphics console's rendering buffer on its first use.
STATIC EFI_STATUS ConfigureTextOutput (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *Out,
                                      OPPO_TEXT_SETUP *Setup)
{
  UINTN Index, Columns, Rows, BestArea = 0;
  EFI_STATUS Status;
  Setup->InitialMode = Setup->SelectedMode = -1;
  Setup->Columns = Setup->Rows = 0;
  Setup->SetStatus = Setup->ClearStatus = EFI_NOT_STARTED;
  if (Out == NULL || Out->Mode == NULL || Out->QueryMode == NULL ||
      Out->SetMode == NULL || Out->ClearScreen == NULL || Out->OutputString == NULL)
    return EFI_UNSUPPORTED;
  Setup->InitialMode = Out->Mode->Mode;
  // Bound enumeration and multiplication for a malformed protocol instance.
  if (Out->Mode->MaxMode <= 0 || Out->Mode->MaxMode > 128) return EFI_UNSUPPORTED;
  for (Index = 0; Index < (UINTN)Out->Mode->MaxMode; Index++) {
    Status = Out->QueryMode (Out, Index, &Columns, &Rows);
    if (EFI_ERROR (Status) || Columns == 0 || Rows == 0 || Columns > 512 || Rows > 512)
      continue;
    if (Columns * Rows > BestArea) {
      BestArea = Columns * Rows;
      Setup->SelectedMode = (INT32)Index;
      Setup->Columns = Columns; Setup->Rows = Rows;
    }
  }
  if (Setup->SelectedMode < 0) return EFI_UNSUPPORTED;
  Setup->SetStatus = Out->SetMode (Out, (UINTN)Setup->SelectedMode);
  if (EFI_ERROR (Setup->SetStatus)) return Setup->SetStatus;
  if (Out->Mode->Mode != Setup->SelectedMode) return EFI_DEVICE_ERROR;
  Setup->ClearStatus = Out->ClearScreen (Out);
  return Setup->ClearStatus;
}

STATIC VOID EFIAPI Timeout (EFI_EVENT Event, VOID *Context)
{
  (VOID)Event; (VOID)Context;
  DEBUG ((DEBUG_INFO, "BDS timer callback: reset requested\n"));
  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
}

STATIC VOID StartDeadline (VOID)
{
  UINT64 First, Last;
  UINT64 Frequency = GetPerformanceCounterProperties (&First, &Last);
  EFI_EVENT Event;
  EFI_STATUS Status;
  mCounterDeadlineReady = FALSE;
  mTimerStatus = EFI_NOT_STARTED;
  // The validated ARM counter counts upward. Do not guess for another source.
  if (First == 0 && Last == MAX_UINT64 && Frequency != 0 && Frequency <= 1000000000ULL) {
    mCounterStart = GetPerformanceCounter ();
    mCounterTicks = Frequency * DeadlineSeconds ();
    mCounterDeadlineReady = TRUE;
  }
  DEBUG ((DEBUG_INFO, "BDS IRQ enabled=%u; counter=%Lu Hz; fallback=%u\n",
          (UINT32)ArmGetInterruptState (), Frequency, (UINT32)mCounterDeadlineReady));
  Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, Timeout, NULL, &Event);
  mEventStatus = Status;
  DEBUG ((DEBUG_INFO, "BDS CreateEvent: %r\n", Status));
  if (!EFI_ERROR (Status)) {
    Status = gBS->SetTimer (Event, TimerRelative, (UINT64)DeadlineSeconds () * 10000000);
    mTimerStatus = Status;
    DEBUG ((DEBUG_INFO, "BDS SetTimer: %r\n", Status));
    if (EFI_ERROR (Status)) gBS->CloseEvent (Event);
  }
}

STATIC VOID CheckDeadline (VOID)
{
  // This also works without event delivery after control returns to BDS.
  // It cannot rescue a blocked driver or a Shell that never returns.
  if (mCounterDeadlineReady && GetPerformanceCounter () - mCounterStart >= mCounterTicks) {
    DEBUG ((DEBUG_INFO, "BDS polled counter deadline: reset requested\n"));
    gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
  }
}

STATIC EFI_STATUS ProbeProtocol (EFI_GUID *Guid, CONST CHAR8 *Name)
{
  VOID *Interface;
  EFI_STATUS Status = gBS->LocateProtocol (Guid, NULL, &Interface);
  DEBUG ((DEBUG_INFO, "BDS %a: %r\n", Name, Status));
  return Status;
}

STATIC VOID EFIAPI Entry (EFI_BDS_ARCH_PROTOCOL *This)
{
  EFI_HANDLE *Handles, Shell;
  UINTN Count, Index;
  EFI_STATUS Status;
  EFI_STATUS ConsoleStatus = EFI_NOT_FOUND, HiiDatabase, HiiFont, HiiString;
  OPPO_TEXT_SETUP Text = {-1, -1, 0, 0, EFI_NOT_STARTED, EFI_NOT_STARTED};
  (VOID)This;
  if (!LabIdentityValid ()) {
    gRT->ResetSystem (EfiResetCold, EFI_COMPROMISED_DATA, 0, NULL);
    for (;;) gBS->Stall (100000);
  }
  DEBUG ((DEBUG_INFO, "BDS entered\n"));
  StartDeadline ();
  // A visible stage marker if connecting the console stalls before text.
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  if (gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop) == EFI_SUCCESS &&
      Gop->Mode != NULL && Gop->Mode->Info != NULL &&
      Gop->Mode->Info->HorizontalResolution >= 1080 &&
      Gop->Mode->Info->VerticalResolution >= 280) {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Yellow = {0, 255, 255, 0};
    Gop->Blt (Gop, &Yellow, EfiBltVideoFill, 0, 0, 0, 256, 1080, 24, 0);
  }
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  if (gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &Count, &Handles) == EFI_SUCCESS) {
    DEBUG ((DEBUG_INFO, "BDS connect %u handles\n", (UINT32)Count));
    for (Index = 0; Index < Count; Index++) {
      CheckDeadline ();
      Status = gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
      DEBUG ((DEBUG_INFO, "BDS connect %u: %r\n", (UINT32)Index, Status));
    }
    FreePool (Handles);
  }
  if (gBS->LocateHandleBuffer (ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &Count, &Handles) == EFI_SUCCESS) {
    for (Index = 0; Index < Count; Index++) {
      EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *Out;
      gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
      if (gBS->HandleProtocol (Handles[Index], &gEfiSimpleTextOutProtocolGuid, (VOID **)&Out) == EFI_SUCCESS) {
        ConsoleStatus = ConfigureTextOutput (Out, &Text);
        DEBUG ((DEBUG_INFO, "BDS text setup: %r; SetMode: %r; ClearScreen: %r\n",
                ConsoleStatus, Text.SetStatus, Text.ClearStatus));
        if (EFI_ERROR (ConsoleStatus)) continue;
        gST->ConsoleOutHandle = Handles[Index]; gST->ConOut = Out;
        gST->StandardErrorHandle = Handles[Index]; gST->StdErr = Out;
        Out->OutputString (Out, L"OPPO CPH2205 - experimental native EDK2\r\n");
        Out->OutputString (Out, FeaturePcdGet (PcdNativeExtraMemoryLab) ?
          L"SD memory lab: reset requested after 1800 seconds.\r\n\r\n" :
          L"Laboratory timer: reset requested after 90 seconds.\r\n\r\n");
        break;
      }
    }
    FreePool (Handles);
  }
  DEBUG ((DEBUG_INFO, "BDS console pointers: input=%u output=%u\n",
          (UINT32)(gST->ConIn != NULL), (UINT32)(gST->ConOut != NULL)));
  HiiDatabase = ProbeProtocol (&gEfiHiiDatabaseProtocolGuid, "HII database");
  HiiFont = ProbeProtocol (&gEfiHiiFontProtocolGuid, "HII font");
  HiiString = ProbeProtocol (&gEfiHiiStringProtocolGuid, "HII string");
  gST->Hdr.CRC32 = 0;
  gBS->CalculateCrc32 (gST, gST->Hdr.HeaderSize, &gST->Hdr.CRC32);
  if (FeaturePcdGet (PcdNativeAcpiProfileLab) && !EFI_ERROR (ConsoleStatus)) OppoAcpiProbeLab ();
  if (FeaturePcdGet (PcdNativeSdReadLab) && !EFI_ERROR (ConsoleStatus)) {
    EFI_STATUS Platform = FeaturePcdGet (PcdNativeSdAcpiBootLab) ? OppoAcpiPrepareForOs () : EFI_SUCCESS;
    DEBUG ((DEBUG_INFO, "BDS SD PLATFORM GATE=%r\n", Platform));
    if (Platform == EFI_SUCCESS) OppoSdReadLab (mImage);
  }
  // Do not invoke Shell with an uninitialized graphical text console.
  Status = (FeaturePcdGet (PcdNativeSdReadLab) || FeaturePcdGet (PcdNativeAcpiProfileLab)) ? EFI_ABORTED : EFI_ERROR (ConsoleStatus) ? ConsoleStatus :
    gBS->LocateHandleBuffer (ByProtocol, &gEfiFirmwareVolume2ProtocolGuid, NULL, &Count, &Handles);
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
        DEBUG ((DEBUG_INFO, "BDS starting Shell application\n"));
        Status = gBS->StartImage (Shell, NULL, NULL);
        // The generic DXE core unloads an application when StartImage returns.
        DEBUG ((DEBUG_INFO, "BDS Shell StartImage returned: %r\n", Status));
      }
      break;
    }
    FreePool (Handles);
  }
  if (!EFI_ERROR (ConsoleStatus))
    gST->ConOut->OutputString (gST->ConOut, FeaturePcdGet (PcdNativeAcpiProfileLab) ?
      (FeaturePcdGet (PcdNativeCpuProfileLab) ? L"Read-only CPU profile lab complete.\r\n" :
       FeaturePcdGet (PcdNativeAcpiInstallLab) ? L"ACPI install lab complete.\r\n" :
       L"Read-only ACPI profile lab complete.\r\n") : FeaturePcdGet (PcdNativeSdReadLab) ?
      L"microSD read lab complete.\r\n" : L"Shell returned or could not start.\r\n");
  // Repeat the useful context after Shell, whose ClearScreen may erase it.
  DEBUG ((DEBUG_INFO, "BDS text initial=%d selected=%d size=%ux%u setup=%r\n",
          Text.InitialMode, Text.SelectedMode, (UINT32)Text.Columns, (UINT32)Text.Rows, ConsoleStatus));
  DEBUG ((DEBUG_INFO, "BDS SetMode=%r ClearScreen=%r; input=%u\n",
          Text.SetStatus, Text.ClearStatus, (UINT32)(gST->ConIn != NULL)));
  DEBUG ((DEBUG_INFO, "BDS HII database=%r font=%r string=%r\n", HiiDatabase, HiiFont, HiiString));
  DEBUG ((DEBUG_INFO, "BDS IRQ=%u event=%r timer=%r counter fallback=%u\n",
          (UINT32)ArmGetInterruptState (), mEventStatus, mTimerStatus, (UINT32)mCounterDeadlineReady));
  for (;;) {
    CheckDeadline ();
    gBS->Stall (100000);
  }
}

STATIC EFI_BDS_ARCH_PROTOCOL mBds = {Entry};

EFI_STATUS EFIAPI OppoBdsEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_HANDLE Handle = NULL;
  (VOID)SystemTable;
  mImage = ImageHandle;
  return gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiBdsArchProtocolGuid, &mBds, NULL);
}
