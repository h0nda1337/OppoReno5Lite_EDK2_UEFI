/* SPDX-License-Identifier: BSD-2-Clause */
/* Exercise the actual bounded file benchmark with synthetic streams/counter. */
#include <Uefi.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _PCD_GET_MODE_BOOL_PcdNativeSdBootLab FALSE
#define _PCD_GET_MODE_BOOL_PcdNativeSdFastReadLab FALSE
#include "../OppoReno5LitePkg/Drivers/OppoBdsDxe/SdRead.c"
STATIC EFI_FILE_PROTOCOL mRoot,mFile;
STATIC UINTN mCase,mOpens,mCloses,mReads,mAllocated;
STATIC UINT64 mTick;
VOID *EFIAPI ZeroMem(VOID *P,UINTN N) {return memset(P,0,N);}
VOID *EFIAPI AllocatePool(UINTN N)
{assert(N==65536);if(mCase==2)return NULL;mAllocated++;return malloc(N);}
VOID EFIAPI FreePool(VOID *P) {assert(P && mAllocated);mAllocated--;free(P);}
UINT64 EFIAPI GetPerformanceCounterProperties(UINT64 *First,UINT64 *Last)
{*First=mCase==7 ? MAX_UINT64 : 0;*Last=mCase==7 ? 0 : MAX_UINT64;return mCase==8 ? 0 : mCase==9 ? 1000000001ULL : 1000000;}
UINT64 EFIAPI GetPerformanceCounter(VOID) {return mCase==10 ? 1 : mTick;}
STATIC EFI_STATUS EFIAPI Open(EFI_FILE_PROTOCOL *This,EFI_FILE_PROTOCOL **File,
  CHAR16 *Name,UINT64 Mode,UINT64 Attributes)
{
  assert(This==&mRoot && Mode==EFI_FILE_MODE_READ && Attributes==0);
  assert(!memcmp(Name,L"\\sources\\boot.wim",sizeof(L"\\sources\\boot.wim")));
  mOpens++;*File=mCase==12 ? NULL : &mFile;return mCase==1 ? EFI_NOT_FOUND : EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Close(EFI_FILE_PROTOCOL *This)
{assert(This==&mFile);mCloses++;return mCase==11 ? EFI_DEVICE_ERROR : EFI_SUCCESS;}
STATIC EFI_STATUS EFIAPI Read(EFI_FILE_PROTOCOL *This,UINTN *Bytes,VOID *Buffer)
{
  assert(This==&mFile && *Bytes>0 && *Bytes<=65536 && Buffer);mReads++;mTick+=1000;
  if(mCase==3 && mReads==3)return EFI_CRC_ERROR;
  if(mCase==4) {*Bytes+=1;return EFI_SUCCESS;}
  if(mCase==5) {*Bytes=0;return EFI_SUCCESS;}
  if(mCase==6)*Bytes=1024;
  if(mCase==14 && mReads==1)*Bytes=65536-384;
  memset(Buffer,0,*Bytes);return EFI_SUCCESS;
}
int main(void)
{
  mRoot.Open=Open;mFile.Read=Read;mFile.Close=Close;
  for(mCase=0;mCase<15;mCase++) {
    mOpens=mCloses=mReads=mAllocated=0;mTick=mCase==13 ? MAX_UINT64-7 : 123;
    OPPO_SD_BENCHMARK B;EFI_STATUS S=BenchmarkSd(&mRoot,&B);
    EFI_STATUS Expected=mCase==1 ? EFI_NOT_FOUND : mCase==2 ? EFI_OUT_OF_RESOURCES :
      mCase==3 ? EFI_CRC_ERROR : mCase==4 || mCase==5 ? EFI_BAD_BUFFER_SIZE :
      mCase==6 || mCase==10 ? EFI_NOT_READY : mCase>=7 && mCase<=9 ? EFI_UNSUPPORTED :
      mCase==11 || mCase==12 ? EFI_DEVICE_ERROR : EFI_SUCCESS;
    assert(S==Expected && mAllocated==0);
    assert(mOpens==(mCase>=7 && mCase<=9 ? 0U : 1U));
    assert(mCloses==(mOpens && mCase!=1 && mCase!=12 ? 1U : 0U));
    assert(mReads<=1024 && B.Bytes<=16777216);
    if(S==EFI_SUCCESS) {
      assert(B.Bytes==16777216 && B.ElapsedTicks==mReads*1000 && B.CounterHz==1000000);
      assert(B.KiBPerSecond==16384ULL*1000000/(mReads*1000));
      // Independent repeated-squaring oracle for FNV over a zero byte stream.
      UINT64 P=1099511628211ULL,Power=1;UINTN N=16777216;
      while(N) {if(N&1)Power*=P;P*=P;N>>=1;}
      assert(B.Hash==14695981039346656037ULL*Power);
    }
    if(mCase==6)assert(mReads==1024 && B.Bytes==1048576);
  }
  OPPO_SD_BENCHMARK B;
  assert(BenchmarkSd(NULL,&B)==EFI_INVALID_PARAMETER);
  assert(BenchmarkSd(&mRoot,NULL)==EFI_INVALID_PARAMETER);
  mRoot.Open=NULL;assert(BenchmarkSd(&mRoot,&B)==EFI_INVALID_PARAMETER);
  puts("SD benchmark: 15 synthetic stream/counter cases, read-only open, bounded calls/allocations, FNV zero-stream oracle, short/error reads, cleanup and counter wrap passed; no physical throughput measured.");
  return 0;
}
