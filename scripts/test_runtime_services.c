/* SPDX-License-Identifier: BSD-2-Clause */
/* Actual original driver; synthetic service tables, no firmware or hardware. */
#include <assert.h>
#include <stdarg.h>
#include <Uefi.h>
#include <setjmp.h>
#include <stdio.h>
STATIC BOOLEAN mBootLab;
#define _PCD_GET_MODE_BOOL_PcdNativeSdBootLab mBootLab
#include "../OppoReno5LitePkg/Drivers/OppoRuntimeDxe/OppoRuntimeDxe.c"

EFI_GUID gEfiResetArchProtocolGuid, gEfiRealTimeClockArchProtocolGuid, gEfiCapsuleArchProtocolGuid;
EFI_GUID gEfiEventExitBootServicesGuid;
STATIC EFI_RUNTIME_SERVICES mRuntime;
STATIC EFI_BOOT_SERVICES mBoot;
EFI_RUNTIME_SERVICES *gRT = &mRuntime;
EFI_BOOT_SERVICES *gBS = &mBoot;
STATIC jmp_buf mResetJump;
STATIC UINTN mExpectedSmc, mTrace, mEvents, mMode;
STATIC EFI_RESET_TYPE mRequestedType;
STATIC EFI_EVENT_NOTIFY mExitNotify;
UINTN EFIAPI ArmCallSmc0(UINTN Function, UINTN *A, UINTN *B, UINTN *C)
{assert(Function==mExpectedSmc && !A && !B && !C && mTrace==(mMode==1 ? 3U : 0U));longjmp(mResetJump,1);return 0;}
VOID EFIAPI CpuDeadLoop(VOID) { assert(0); }
UINTN EFIAPI ArmDisableInterrupts(VOID) {assert(mTrace==0 && mMode==1);mTrace=1;return 1;}
VOID EFIAPI OppoBootLogReset(EFI_RESET_TYPE Type, EFI_STATUS Status)
{assert(mTrace==1 && Type==mRequestedType && Status==EFI_ABORTED);mTrace=2;}
UINTN EFIAPI MicroSecondDelay(UINTN Us)
{assert(mTrace==2 && Us==15000000);mTrace=3;return Us;}
STATIC EFI_STATUS EFIAPI CreateExit(UINT32 Type, EFI_TPL Tpl, EFI_EVENT_NOTIFY Notify,
  CONST VOID *Context, CONST EFI_GUID *Group, EFI_EVENT *Event)
{
  assert(Type==EVT_NOTIFY_SIGNAL && Tpl==TPL_NOTIFY && Notify && !Context);
  assert(Group==&gEfiEventExitBootServicesGuid);mEvents++;mExitNotify=Notify;
  if(mMode==3)return EFI_OUT_OF_RESOURCES;
  *Event=(EFI_EVENT)(UINTN)2;return EFI_SUCCESS;
}

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
  mBoot.CreateEventEx=CreateExit;
  for(mMode=0;mMode<4;mMode++)for(mRequestedType=EfiResetCold;mRequestedType<=EfiResetPlatformSpecific;mRequestedType++) {
    mBootLab=mMode!=0;mTrace=mEvents=0;mExitNotify=NULL;gBS=&mBoot;
    assert(OppoRuntimeEntry(NULL,NULL)==EFI_SUCCESS && mEvents==(mBootLab ? 1U : 0U));
    if(mMode==2) {assert(mExitNotify);mExitNotify((EFI_EVENT)(UINTN)2,NULL);}
    assert(mBootResetDiagnostic==(mMode==1));
    mExpectedSmc=mRequestedType==EfiResetShutdown ? ARM_SMC_ID_PSCI_SYSTEM_OFF : ARM_SMC_ID_PSCI_SYSTEM_RESET;
    gBS=NULL; // Reset and its capture path must not call any Boot Service.
    if(setjmp(mResetJump)==0)gRT->ResetSystem(mRequestedType,EFI_ABORTED,0,NULL);
    assert(!mBootResetDiagnostic);
  }
  puts("Runtime reset: 16 host cases preserve shutdown/reset SMC, boot-only bounded capture, no Boot Services in Reset, ExitBS disables display and event-registration failure skips diagnostics; no hardware execution.");
  return 0;
}
