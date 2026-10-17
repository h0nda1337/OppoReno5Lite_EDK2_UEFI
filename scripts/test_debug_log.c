/* SPDX-License-Identifier: BSD-2-Clause */
/* Real logger and framebuffer code, with guarded synthetic host mappings.
 * Formatting is stubbed here; production uses the generic EDK2 PrintLib. */
#define _GNU_SOURCE
#include <Uefi.h>
#include <OppoNativeProfile.h>
#include <Library/OppoDebugLog.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <sys/mman.h>

STATIC jmp_buf mAssertJump;
STATIC BOOLEAN mTrapAssert;
VOID EFIAPI MemoryFence(VOID) { __sync_synchronize(); }
VOID *EFIAPI ZeroMem(VOID *Buffer, UINTN Bytes) { return memset(Buffer,0,Bytes); }
VOID *EFIAPI SetMem(VOID *Buffer, UINTN Bytes, UINT8 Value) { return memset(Buffer,Value,Bytes); }
VOID EFIAPI CpuDeadLoop(VOID) { if (mTrapAssert) longjmp(mAssertJump,1); abort(); }
UINTN EFIAPI AsciiVSPrint(CHAR8 *Buffer, UINTN Bytes, CONST CHAR8 *Format, VA_LIST Marker)
{
  UINTN used=0;
  for(UINTN i=0;Format[i] && used+1<Bytes;i++) {
    if(Format[i]!='%') { Buffer[used++]=Format[i];continue; }
    CHAR8 number[32];CONST CHAR8 *text;
    if(Format[++i]=='a')text=VA_ARG(Marker,CONST CHAR8 *);
    else if(Format[i]=='u') {
      snprintf(number,sizeof number,"%u",VA_ARG(Marker,unsigned int));text=number;
    } else { assert(Format[i]=='%');text="%"; }
    for(UINTN j=0;text[j] && used+1<Bytes;j++)Buffer[used++]=text[j];
  }
  Buffer[used]=0;return used;
}
UINTN EFIAPI AsciiBSPrint(CHAR8 *Buffer, UINTN Bytes, CONST CHAR8 *Format, BASE_LIST Marker)
{
  (VOID)Marker;UINTN n=strlen(Format);if(n>=Bytes)n=Bytes-1;
  memcpy(Buffer,Format,n);Buffer[n]=0;return n;
}
RETURN_STATUS EFIAPI OppoDebugConstructor(VOID);

STATIC VOID *guarded(UINTN Base, UINTN Bytes)
{
  assert(Bytes%4096==0 && Base%4096==0);
  VOID *p=mmap((VOID *)(Base-4096),Bytes+8192,PROT_NONE,
               MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
  if(p==MAP_FAILED) { perror("synthetic mapping");exit(1); }
  assert(mprotect((VOID *)Base,Bytes,PROT_READ|PROT_WRITE)==0);
  return (VOID *)Base;
}

int main(void)
{
  UINTN bytes=OPPO_NATIVE_FB_STRIDE*OPPO_NATIVE_FB_HEIGHT;
  UINT8 *frame=guarded(OPPO_NATIVE_FB_BASE,bytes);
  UINT8 *state=guarded(OPPO_NATIVE_DEBUG_BASE,4096);
  UINT8 *before=malloc(bytes);assert(before);
  memset(frame,0xa5,bytes);
  assert(OppoDebugConstructor()==RETURN_SUCCESS);
  DebugPrint(DEBUG_INFO,"BEFORE VALIDATION");
  for(UINTN i=0;i<bytes;i++)assert(frame[i]==0xa5);
  assert(OppoDebugStart());
  DebugPrint(DEBUG_INFO,"HELLO %u\n",123U);
  memcpy(before,frame,bytes);
  assert(OppoDebugConstructor()==RETURN_SUCCESS);
  assert(memcmp(before,frame,bytes)==0); /* Core binding keeps the SEC log. */
  UINT8 saved_state[4096];memcpy(saved_state,state,4096);
  memset(state,0,4096);assert(OppoDebugConstructor()==RETURN_SUCCESS);
  memcpy(state,saved_state,4096);DebugPrint(DEBUG_INFO,"EARLY CORE BIND\n");
#ifdef OPPO_LOG_EARLY_CORE
  assert(memcmp(before,frame,bytes)!=0);
#else
  assert(memcmp(before,frame,bytes)==0);
  assert(OppoDebugConstructor()==RETURN_SUCCESS);
#endif
  DebugBPrint(DEBUG_INFO,"BASE-LIST TEXT\n",NULL);
  CHAR8 long_text[1024];memset(long_text,'Z',sizeof long_text);long_text[1023]=0;
  DebugPrint(DEBUG_INFO,"%a",long_text);
  for(UINTN line=0;line<140;line++)DebugPrint(DEBUG_INFO,"LINE %u\t[]_/?\n",(unsigned)line);
  DebugPrint(DEBUG_ERROR,"ERROR\n");
  BOOLEAN red=FALSE;
  for(UINTN y=0;y<OPPO_NATIVE_FB_HEIGHT;y++) {
    for(UINTN x=0;x<OPPO_NATIVE_FB_STRIDE;x++) {
      if(y<320 || x>=OPPO_NATIVE_FB_WIDTH*4)assert(frame[y*OPPO_NATIVE_FB_STRIDE+x]==0xa5);
    }
    if(y>=320)for(UINTN x=0;x<OPPO_NATIVE_FB_WIDTH;x++)
      if(((UINT32 *)(frame+y*OPPO_NATIVE_FB_STRIDE))[x]==0xffff0000)red=TRUE;
  }
  assert(red);
  memcpy(before,frame,bytes);
  DebugPrint(DEBUG_VERBOSE,"FILTERED");DebugPrint(DEBUG_INFO,NULL);
  assert(memcmp(before,frame,bytes)==0);
  memset(state,0,4096);DebugPrint(DEBUG_INFO,"INVALID STATE");
  assert(memcmp(before,frame,bytes)==0);
  assert(OppoDebugStart());
  UINT8 clear[5]={1,2,3,4,5};DebugClearMemory(clear+1,3);
  assert(clear[0]==1 && clear[1]==0xaf && clear[3]==0xaf && clear[4]==5);
  mTrapAssert=TRUE;
  if(setjmp(mAssertJump)==0) { DebugAssert("/private/build/path/Example.c",42,"TEST ASSERT");abort(); }
  free(before);
  assert(munmap(frame-4096,bytes+8192)==0);
  assert(munmap(state-4096,12288)==0);
  puts("Original display logger: guarded mappings, wrapping, padding, truncation, core binding and assertion stop passed with host sanitizers; no hardware test.");
  return 0;
}
