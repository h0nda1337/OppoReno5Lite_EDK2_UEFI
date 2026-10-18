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
VOID EFIAPI MemoryFence(VOID) {__sync_synchronize();}
UINTN EFIAPI AsciiSPrint(CHAR8 *Buffer, UINTN Bytes, CONST CHAR8 *Format, ...)
{
  va_list Args;va_start(Args,Format);
  assert(strstr(Format,"TYPE=%u STATUS=%016Lx") && Bytes==192);
  assert(va_arg(Args,UINT32)==mExpectedType && va_arg(Args,UINT64)==mExpectedStatus);
  CONST CHAR8 *Kind=va_arg(Args,CONST CHAR8 *);
  assert(!strcmp(Kind,mExpectedType==EfiResetShutdown ? "POWER OFF" : "RESET"));
  va_end(Args);mFormats++;
  return (UINTN)snprintf(Buffer,Bytes,"OPPO BOOT RESET REQUEST\nTYPE=%u STATUS=%016llx\nREQUEST=%s\n15 SECOND CAPTURE HOLD\n",
    mExpectedType,(unsigned long long)mExpectedStatus,Kind);
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
  for(UINTN Case=0;Case<6;Case++) {
    memset(Frame,0xa5,Bytes);memset(S,0,4096);mFormats=0;
    assert(OppoFbInitialize(&S->Frame,Frame,Bytes,&Layout));
    S->Signature=Case==4 ? 0 : LOG_SIGNATURE;S->Revision=1;S->Busy=Case==5;
    mExpectedType=(EFI_RESET_TYPE)(Case%4);mExpectedStatus=Case%2 ? EFI_ABORTED : EFI_SUCCESS;
    OppoBootLogReset(mExpectedType,mExpectedStatus);
    assert(mFormats==(Case<4 ? 1U : 0U));
    UINTN Changed=0;
    for(UINTN Y=0;Y<OPPO_NATIVE_FB_HEIGHT;Y++)for(UINTN X=0;X<OPPO_NATIVE_FB_STRIDE;X++) {
      UINT8 V=Frame[Y*OPPO_NATIVE_FB_STRIDE+X];
      if(Case>=4 || Y<FIRST_ROW+(ROWS-20)*CELL_HEIGHT || X>=OPPO_NATIVE_FB_WIDTH*4)assert(V==0xa5);
      if(V!=0xa5)Changed++;
    }
    assert((Changed!=0)==(Case<4));
  }
  assert(munmap(Frame-4096,Bytes+8192)==0 && munmap((UINT8 *)S-4096,12288)==0);
  puts("Boot reset report: 6 guarded host framebuffer cases, original glyphs, lower-screen bounded writes, invalid/busy state rejected; formatting stubbed and no hardware execution.");
  return 0;
}
