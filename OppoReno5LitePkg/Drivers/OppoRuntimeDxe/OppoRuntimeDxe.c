/** @file
  Original PSCI reset service and explicit unsupported RTC for early bring-up.
  No storage, PMIC or clock register access. RTC and capsule updates are unsupported.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Uefi.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Protocol/RealTimeClock.h>
#include <Protocol/Reset.h>
#include <Protocol/Capsule.h>
#include <Guid/EventGroup.h>
#include <Library/ArmSmcLib.h>
#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/OppoBootLogLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

STATIC BOOLEAN mBootResetDiagnostic;
STATIC VOID EFIAPI EndResetDiagnostic (EFI_EVENT Event, VOID *Context)
{ (VOID)Event; (VOID)Context; mBootResetDiagnostic = FALSE; }

STATIC VOID EFIAPI Reset (EFI_RESET_TYPE Type, EFI_STATUS Status, UINTN Bytes, VOID *Data)
{
  (VOID)Bytes; (VOID)Data;
  if (mBootResetDiagnostic) {
    mBootResetDiagnostic = FALSE;
    // Final pre-reset path: exclude the BDS timer and framebuffer writers.
    // The reporter and counter delay use no Boot Services or allocations.
    ArmDisableInterrupts ();
    OppoBootLogReset (Type, Status);
    MicroSecondDelay (15000000);
  }
  ArmCallSmc0 (Type == EfiResetShutdown ? ARM_SMC_ID_PSCI_SYSTEM_OFF : ARM_SMC_ID_PSCI_SYSTEM_RESET,
               NULL, NULL, NULL);
  CpuDeadLoop ();
}

STATIC EFI_STATUS EFIAPI GetTime (EFI_TIME *Time, EFI_TIME_CAPABILITIES *Capabilities)
{
  (VOID)Time; (VOID)Capabilities;
  return EFI_UNSUPPORTED;
}
STATIC EFI_STATUS EFIAPI SetTime (EFI_TIME *Time)
{
  (VOID)Time; return EFI_UNSUPPORTED;
}
STATIC EFI_STATUS EFIAPI GetWakeup (BOOLEAN *Enabled, BOOLEAN *Pending, EFI_TIME *Time)
{
  (VOID)Enabled; (VOID)Pending; (VOID)Time; return EFI_UNSUPPORTED;
}
STATIC EFI_STATUS EFIAPI SetWakeup (BOOLEAN Enabled, EFI_TIME *Time)
{
  (VOID)Enabled; (VOID)Time; return EFI_UNSUPPORTED;
}

STATIC EFI_STATUS EFIAPI UpdateCapsule (EFI_CAPSULE_HEADER **Headers, UINTN Count,
                                       EFI_PHYSICAL_ADDRESS ScatterGather)
{
  (VOID)ScatterGather;
  if (Headers == NULL || Count == 0) return EFI_INVALID_PARAMETER;
  // This laboratory port never consumes, queues or writes capsule payloads.
  return EFI_UNSUPPORTED;
}

STATIC EFI_STATUS EFIAPI QueryCapsule (EFI_CAPSULE_HEADER **Headers, UINTN Count,
                                      UINT64 *MaximumSize, EFI_RESET_TYPE *ResetType)
{
  if (Headers == NULL || Count == 0 || MaximumSize == NULL || ResetType == NULL)
    return EFI_INVALID_PARAMETER;
  *MaximumSize = 0;
  *ResetType = EfiResetCold;
  return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI OppoRuntimeEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_HANDLE Handle = NULL;
  EFI_EVENT Event;
  EFI_STATUS Status;
  mBootResetDiagnostic = FALSE;
  (VOID)ImageHandle; (VOID)SystemTable;
  gRT->ResetSystem = Reset;
  gRT->GetTime = GetTime; gRT->SetTime = SetTime;
  gRT->GetWakeupTime = GetWakeup; gRT->SetWakeupTime = SetWakeup;
  gRT->UpdateCapsule = UpdateCapsule; gRT->QueryCapsuleCapabilities = QueryCapsule;
  Status = gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiResetArchProtocolGuid, NULL,
    &gEfiRealTimeClockArchProtocolGuid, NULL, &gEfiCapsuleArchProtocolGuid, NULL, NULL);
  if (!EFI_ERROR (Status) && FeaturePcdGet (PcdNativeSdBootLab)) {
    // Never touch physical display pointers from a runtime reset after ExitBS.
    // If registration fails, retain the normal reset with no diagnostic delay.
    EFI_STATUS Registered = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_NOTIFY,
      EndResetDiagnostic, NULL, &gEfiEventExitBootServicesGuid, &Event);
    mBootResetDiagnostic = (BOOLEAN)!EFI_ERROR (Registered);
  }
  return Status;
}
