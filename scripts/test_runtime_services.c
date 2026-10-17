/* SPDX-License-Identifier: BSD-2-Clause */
/* Actual original driver; synthetic service tables, no firmware or hardware. */
#include <assert.h>
#include <stdarg.h>
#include "../OppoReno5LitePkg/Drivers/OppoRuntimeDxe/OppoRuntimeDxe.c"

EFI_GUID gEfiResetArchProtocolGuid, gEfiRealTimeClockArchProtocolGuid, gEfiCapsuleArchProtocolGuid;
STATIC EFI_RUNTIME_SERVICES mRuntime;
STATIC EFI_BOOT_SERVICES mBoot;
EFI_RUNTIME_SERVICES *gRT = &mRuntime;
EFI_BOOT_SERVICES *gBS = &mBoot;
UINTN EFIAPI ArmCallSmc0(UINTN Function, UINTN *A, UINTN *B, UINTN *C)
{ (VOID)Function; (VOID)A; (VOID)B; (VOID)C; assert(0); return 0; }
VOID EFIAPI CpuDeadLoop(VOID) { assert(0); }

STATIC EFI_STATUS EFIAPI Install(EFI_HANDLE *Handle, ...)
{
  EFI_GUID *expected[]={&gEfiResetArchProtocolGuid,&gEfiRealTimeClockArchProtocolGuid,&gEfiCapsuleArchProtocolGuid};
  va_list args;va_start(args,Handle);
  assert(gRT->UpdateCapsule && gRT->QueryCapsuleCapabilities && gRT->ResetSystem);
  for(UINTN i=0;i<3;i++) {
    assert(va_arg(args,EFI_GUID *)==expected[i]);assert(va_arg(args,VOID *)==NULL);
  }
  assert(va_arg(args,EFI_GUID *)==NULL);va_end(args);
  *Handle=(EFI_HANDLE)(UINTN)1;return EFI_SUCCESS;
}

int main(void)
{
  mBoot.InstallMultipleProtocolInterfaces=Install;
  assert(OppoRuntimeEntry(NULL,NULL)==EFI_SUCCESS);
  EFI_CAPSULE_HEADER header={0}, *headers[]={&header};
  UINT64 maximum=123;EFI_RESET_TYPE type=EfiResetWarm;
  assert(gRT->UpdateCapsule(NULL,1,0)==EFI_INVALID_PARAMETER);
  assert(gRT->UpdateCapsule(headers,0,0)==EFI_INVALID_PARAMETER);
  assert(gRT->UpdateCapsule(headers,1,MAX_UINT64)==EFI_UNSUPPORTED);
  assert(header.HeaderSize==0 && header.CapsuleImageSize==0 && headers[0]==&header);
  assert(gRT->QueryCapsuleCapabilities(headers,1,NULL,&type)==EFI_INVALID_PARAMETER);
  assert(gRT->QueryCapsuleCapabilities(headers,1,&maximum,NULL)==EFI_INVALID_PARAMETER);
  assert(gRT->QueryCapsuleCapabilities(NULL,1,&maximum,&type)==EFI_INVALID_PARAMETER);
  assert(gRT->QueryCapsuleCapabilities(headers,0,&maximum,&type)==EFI_INVALID_PARAMETER);
  assert(maximum==123 && type==EfiResetWarm);
  assert(gRT->QueryCapsuleCapabilities(headers,1,&maximum,&type)==EFI_UNSUPPORTED);
  assert(maximum==0 && type==EfiResetCold);
  return 0;
}
