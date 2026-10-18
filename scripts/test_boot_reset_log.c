/* SPDX-License-Identifier: BSD-2-Clause */
/* Original guarded host framebuffer test; no firmware or hardware access. */
#define _GNU_SOURCE
#define OPPO_LOG_BOOT_API_ONLY
#include <Uefi.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "../OppoReno5LitePkg/Library/OppoDebugLib/OppoDebugLib.c"
STATIC EFI_RESET_TYPE mExpectedType;
STATIC EFI_STATUS mExpectedStatus;
STATIC UINTN mFormats;
STATIC OPPO_BOOT_FLOW mExpectedFlow;
STATIC UINT32 mExpectedChanged, mExpectedColored;
VOID EFIAPI MemoryFence(VOID) {__sync_synchronize();}
UINTN EFIAPI AsciiSPrint(CHAR8 *Buffer, UINTN Bytes, CONST CHAR8 *Format, ...)
{
  va_list Args;va_start(Args,Format);
  assert(strstr(Format,"EFI STATUS=%016Lx") && Bytes==384);
  assert(va_arg(Args,UINT32)==mExpectedType);
  CONST CHAR8 *Kind=va_arg(Args,CONST CHAR8 *);
  assert(!strcmp(Kind,mExpectedType==EfiResetShutdown ? "POWER OFF" : "RESET"));
  assert(va_arg(Args,UINT64)==mExpectedStatus && va_arg(Args,UINT32)==mExpectedFlow.Stage);
  assert(va_arg(Args,UINT64)==mExpectedFlow.LoadStatus && va_arg(Args,UINT64)==mExpectedFlow.StartStatus);
  assert(va_arg(Args,UINT64)==mExpectedFlow.MapStatus && va_arg(Args,UINT64)==mExpectedFlow.ConventionalBytes/(1024*1024));
  assert(va_arg(Args,UINT32)==mExpectedChanged && va_arg(Args,UINT32)==mExpectedColored);
  va_end(Args);mFormats++;
  return (UINTN)snprintf(Buffer,Bytes,"OPPO RESET CAPTURE\r\nTYPE=%u REQUEST=%s\r\nEFI STATUS=%016llx\r\nSTAGE=%u LOAD=%016llx\r\nRETURN=%016llx\r\nMAP=%016llx FREE=%lluM\r\nALPHA FIX=%u COLOR=%u\r\n15 SECOND CAPTURE HOLD\r\n",
    mExpectedType,Kind,(unsigned long long)mExpectedStatus,mExpectedFlow.Stage,
    (unsigned long long)mExpectedFlow.LoadStatus,(unsigned long long)mExpectedFlow.StartStatus,
    (unsigned long long)mExpectedFlow.MapStatus,(unsigned long long)(mExpectedFlow.ConventionalBytes/(1024*1024)),
    mExpectedChanged,mExpectedColored);
}
STATIC UINT8 *Guarded(UINTN Base,UINTN Bytes)
{
  assert(Bytes%4096==0);
  VOID *P=mmap((VOID *)(Base-4096),Bytes+8192,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
  assert(P!=MAP_FAILED && mprotect((VOID *)Base,Bytes,PROT_READ|PROT_WRITE)==0);return (UINT8 *)Base;
}
int main(void)
{
  UINTN Bytes=OPPO_NATIVE_FB_STRIDE*OPPO_NATIVE_FB_HEIGHT;
  UINT8 *Frame=Guarded(OPPO_NATIVE_FB_BASE,Bytes);
  LOG_STATE *S=(LOG_STATE *)Guarded(OPPO_NATIVE_DEBUG_BASE,4096);
  OPPO_FB_LAYOUT Layout={OPPO_NATIVE_FB_WIDTH,OPPO_NATIVE_FB_HEIGHT,OPPO_NATIVE_FB_STRIDE,
    OPPO_NATIVE_FB_RED_SHIFT,OPPO_NATIVE_FB_BLUE_SHIFT,TRUE};
  for(UINTN Case=0;Case<11;Case++) {
    memset(Frame,0xa5,Bytes);memset(S,0,4096);mFormats=0;
    UINT32 Raw=Case==8 ? 0xff123456 : Case==9 ? 0x00ffffff : Case==10 ? 0 : 0xa5a5a5a5;
    for(UINTN Y=0;Y<OPPO_NATIVE_FB_HEIGHT;Y++)for(UINTN X=0;X<OPPO_NATIVE_FB_WIDTH;X++)
      ((UINT32 *)Frame)[Y*(OPPO_NATIVE_FB_STRIDE/4)+X]=Raw;
    assert(OppoFbInitialize(&S->Frame,Frame,Bytes,&Layout));
    S->Signature=Case==4 ? 0 : LOG_SIGNATURE;S->Revision=LOG_REVISION;S->Busy=Case==5;
    S->FlowSignature=Case==6 ? 0 : FLOW_SIGNATURE;
    S->Flow=(OPPO_BOOT_FLOW){OPPO_BOOT_START_ACTIVE,EFI_SUCCESS,EFI_NOT_STARTED,EFI_SUCCESS,33ULL*1024*1024+1234};
    if(Case==7) {S->Flow.MapStatus=EFI_DEVICE_ERROR;S->Flow.ConventionalBytes=0;}
    mExpectedFlow=Case==6 ? (OPPO_BOOT_FLOW){0,EFI_NOT_STARTED,EFI_NOT_STARTED,EFI_NOT_STARTED,0} : S->Flow;
    BOOLEAN Valid=Case!=4 && Case!=5;
    BOOLEAN Repair=Valid && mExpectedFlow.Stage==OPPO_BOOT_START_ACTIVE;
    mExpectedChanged=Repair && (Raw>>24)!=255 ? OPPO_NATIVE_FB_WIDTH*OPPO_NATIVE_FB_HEIGHT : 0;
    mExpectedColored=(Raw & 0x00ffffff) ? mExpectedChanged : 0;
    mExpectedType=(EFI_RESET_TYPE)(Case%4);mExpectedStatus=Case%2 ? EFI_ABORTED : EFI_SUCCESS;
    OppoBootLogReset(mExpectedType,mExpectedStatus);
    assert(mFormats==(Valid ? 1U : 0U));
    UINTN Changed=0, White=0;
    for(UINTN Y=0;Y<OPPO_NATIVE_FB_HEIGHT;Y++)for(UINTN X=0;X<OPPO_NATIVE_FB_STRIDE/4;X++) {
      UINT32 V=((UINT32 *)Frame)[Y*(OPPO_NATIVE_FB_STRIDE/4)+X];
      BOOLEAN Visible=X<OPPO_NATIVE_FB_WIDTH;
      BOOLEAN Panel=Valid && Visible && Y>=RESET_REPORT_TOP && Y<RESET_REPORT_TOP+RESET_REPORT_HEIGHT;
      UINT32 Expected=Visible ? Raw : 0xa5a5a5a5;
      if(Visible && Repair)Expected|=0xff000000;
      if(!Panel)assert(V==Expected);
      if(Panel && V!=Raw)Changed++;
      if(Panel && V==0xffffffff)White++;
    }
    assert((Changed!=0)==Valid && (White!=0)==Valid);
    if(Valid) {
      // PrintLib CRLF must not draw the fallback '?' at end of the first line.
      UINTN Offset=RESET_REPORT_TOP*(OPPO_NATIVE_FB_STRIDE/4)+18*24;
      assert(((UINT32 *)Frame)[Offset]==0xff000000);
    }
    if(Repair) {
      UINT32 Again,Colored;OpaqueCapture(S,&Again,&Colored);assert(!Again && !Colored);
    }
  }
  assert(munmap(Frame-4096,Bytes+8192)==0 && munmap((UINT8 *)S-4096,12288)==0);
  puts("Boot reset report: 11 guarded host cases; RGB and pitch padding preserved by pre-reset alpha capture, opaque input unchanged, copied trace and CRLF rendered, invalid/busy/missing trace guarded; formatting stubbed and no hardware execution.");
  return 0;
}
