/** @file
  Original PSCI reset service and explicit unsupported RTC for early bring-up.
  No storage, PMIC or clock register access. RTC is not implemented yet.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Uefi.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Protocol/RealTimeClock.h>
#include <Protocol/Reset.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

STATIC VOID EFIAPI Reset (EFI_RESET_TYPE Type, EFI_STATUS Status, UINTN Bytes, VOID *Data)
{
  (VOID)Status; (VOID)Bytes; (VOID)Data;
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

EFI_STATUS EFIAPI OppoRuntimeEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_HANDLE Handle = NULL;
  (VOID)ImageHandle; (VOID)SystemTable;
  gRT->ResetSystem = Reset;
  gRT->GetTime = GetTime; gRT->SetTime = SetTime;
  gRT->GetWakeupTime = GetWakeup; gRT->SetWakeupTime = SetWakeup;
  return gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiResetArchProtocolGuid, NULL,
                                                 &gEfiRealTimeClockArchProtocolGuid, NULL, NULL);
}
