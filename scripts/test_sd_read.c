/* SPDX-License-Identifier: BSD-2-Clause */
/* SD-only filesystem discovery and hostile PE prefixes, without real media. */
#include <PiDxe.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
STATIC BOOLEAN mBootEnabled;
#define _PCD_GET_MODE_BOOL_PcdNativeSdBootLab mBootEnabled
#include "../OppoReno5LitePkg/Drivers/OppoBdsDxe/SdRead.c"
EFI_GUID gOppoSdStatusProtocolGuid, gOppoSdDeviceGuid;
EFI_GUID gEfiSimpleFileSystemProtocolGuid, gEfiDevicePathProtocolGuid;
STATIC EFI_BOOT_SERVICES mBoot;
EFI_BOOT_SERVICES *gBS=&mBoot;
STATIC OPPO_SD_STATUS mSd;
STATIC EFI_SIMPLE_FILE_SYSTEM_PROTOCOL mFs;
STATIC EFI_FILE_PROTOCOL mRoot, mFile;
STATIC VENDOR_DEVICE_PATH mSdPath, mOtherPath;
STATIC UINTN mScenario, mAllocations, mCloses, mOpens, mReads, mVerified;
STATIC UINTN mPaths, mLoads, mStarts, mUnloads;
STATIC BOOLEAN mAlive;

BOOLEAN EFIAPI DebugPrintEnabled(VOID) {return TRUE;}
BOOLEAN EFIAPI DebugPrintLevelEnabled(UINTN Level) {(VOID)Level;return TRUE;}
VOID EFIAPI DebugPrint(UINTN Level, CONST CHAR8 *Format, ...)
{(VOID)Level;if(strstr(Format,"ARM64 UEFI header verified"))mVerified++;}
VOID *EFIAPI AllocatePool(UINTN Bytes) {mAllocations++;return malloc(Bytes);}
VOID EFIAPI FreePool(VOID *P) {assert(P && mAllocations);mAllocations--;free(P);}
BOOLEAN EFIAPI CompareGuid(CONST GUID *A, CONST GUID *B) {return !memcmp(A,B,sizeof(*A));}
UINTN EFIAPI DevicePathNodeLength(CONST VOID *Node)
{CONST EFI_DEVICE_PATH_PROTOCOL *P=Node;return P->Length[0]|((UINT16)P->Length[1]<<8);}
EFI_DEVICE_PATH_PROTOCOL *EFIAPI FileDevicePath(EFI_HANDLE Medium, CONST CHAR16 *Name)
{
  assert(Medium==(VOID *)2 && mCloses==2 &&
    !memcmp(Name,L"\\EFI\\BOOT\\BOOTAA64.EFI",sizeof(L"\\EFI\\BOOT\\BOOTAA64.EFI")));
  mPaths++;return mScenario==10 ? NULL : AllocatePool(sizeof(EFI_DEVICE_PATH_PROTOCOL));
}
STATIC EFI_STATUS EFIAPI Load(BOOLEAN Policy, EFI_HANDLE Parent,
  EFI_DEVICE_PATH_PROTOCOL *Path, VOID *Source, UINTN Bytes, EFI_HANDLE *Child)
{
  assert(!Policy && Parent==(VOID *)3 && Path && !Source && !Bytes);
  assert(mCloses==2 && mVerified==1 && mAllocations==2);mLoads++;
  if(mScenario==8) {*Child=NULL;return EFI_LOAD_ERROR;}
  if(mScenario==12) {*Child=NULL;return EFI_SECURITY_VIOLATION;}
  if(mScenario==11) {*Child=NULL;return EFI_SUCCESS;}
  *Child=(VOID *)4;mAlive=TRUE;
  return mScenario==9 ? EFI_SECURITY_VIOLATION : EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Start(EFI_HANDLE Child, UINTN *Bytes, CHAR16 **Data)
{
  assert(Child==(VOID *)4 && mAlive && !Bytes && !Data && mCloses==2 && mAllocations==1);
  mStarts++;mAlive=FALSE;return mScenario==13 ? EFI_SUCCESS : EFI_UNSUPPORTED;
}
STATIC EFI_STATUS EFIAPI Unload(EFI_HANDLE Child)
{assert(Child==(VOID *)4 && mAlive && mScenario==9);mAlive=FALSE;mUnloads++;return EFI_SUCCESS;}
STATIC EFI_STATUS EFIAPI Locate(EFI_GUID *Guid, VOID *Registration, VOID **Out)
{assert(Guid==&gOppoSdStatusProtocolGuid && !Registration);*Out=&mSd;return EFI_SUCCESS;}
STATIC EFI_STATUS EFIAPI Handles(EFI_LOCATE_SEARCH_TYPE Type, EFI_GUID *Guid, VOID *Key,
                                UINTN *Count, EFI_HANDLE **Out)
{
  assert(Type==ByProtocol && Guid==&gEfiSimpleFileSystemProtocolGuid && !Key);
  *Count=2;*Out=AllocatePool(2*sizeof(EFI_HANDLE));
  (*Out)[0]=(VOID *)1;(*Out)[1]=(VOID *)2;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Protocol(EFI_HANDLE Handle, EFI_GUID *Guid, VOID **Out)
{
  if(Guid==&gEfiDevicePathProtocolGuid) {*Out=Handle==(VOID *)1 ? &mOtherPath : &mSdPath;return EFI_SUCCESS;}
  // Internal/other storage must never be opened, even though it has a filesystem.
  assert(Handle==(VOID *)2 && Guid==&gEfiSimpleFileSystemProtocolGuid);
  *Out=&mFs;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Volume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This, EFI_FILE_PROTOCOL **Root)
{assert(This==&mFs);*Root=&mRoot;return EFI_SUCCESS;}
STATIC EFI_STATUS EFIAPI Open(EFI_FILE_PROTOCOL *This, EFI_FILE_PROTOCOL **File,
                              CHAR16 *Name, UINT64 Mode, UINT64 Attributes)
{
  assert(This==&mRoot && Mode==EFI_FILE_MODE_READ && !Attributes && Name[0]=='\\');
  mOpens++;*File=&mFile;return mScenario==4 ? EFI_NOT_FOUND : EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Close(EFI_FILE_PROTOCOL *This)
{assert(This==&mFile || This==&mRoot);mCloses++;return EFI_SUCCESS;}
STATIC EFI_STATUS EFIAPI Read(EFI_FILE_PROTOCOL *This, UINTN *Bytes, VOID *Buffer)
{
  UINT8 *P=Buffer;assert(This==&mFile && *Bytes==4096);mReads++;
  memset(P,0,4096);P[0]='M';P[1]='Z';P[0x3c]=0x80;
  P[0x80]='P';P[0x81]='E';P[0x84]=0x64;P[0x85]=0xaa;
  P[0x98]=0x0b;P[0x99]=0x02;P[0xdc]=10;
  if(mScenario==1)memset(P+0x3c,0xff,4);
  if(mScenario==2)*Bytes=10;
  if(mScenario==3 || mScenario==15)P[0x84]=0x4c;
  if(mScenario==5)return EFI_CRC_ERROR;
  return EFI_SUCCESS;
}
int main(void)
{
  gOppoSdDeviceGuid.Data1=1;
  mSdPath.Header.Type=HARDWARE_DEVICE_PATH;mSdPath.Header.SubType=HW_VENDOR_DP;
  mSdPath.Header.Length[0]=sizeof(mSdPath);mSdPath.Guid=gOppoSdDeviceGuid;
  mOtherPath=mSdPath;mOtherPath.Guid.Data1=2;
  mBoot.LocateProtocol=Locate;mBoot.LocateHandleBuffer=Handles;mBoot.HandleProtocol=Protocol;
  mBoot.LoadImage=Load;mBoot.StartImage=Start;mBoot.UnloadImage=Unload;
  mFs.OpenVolume=Volume;mRoot.Open=Open;mRoot.Close=Close;mFile.Close=Close;mFile.Read=Read;
  for(mScenario=0;mScenario<16;mScenario++) {
    mAllocations=mCloses=mOpens=mReads=mVerified=0;
    mPaths=mLoads=mStarts=mUnloads=0;mAlive=FALSE;mBootEnabled=mScenario>=7;
    mSd.Publish=mScenario==6 ? EFI_NOT_STARTED : EFI_SUCCESS;
    OppoSdReadLab(mScenario==14 ? NULL : (VOID *)3);
    assert(mAllocations==0 && !mAlive && mVerified==(mScenario==0 || (mScenario>=7 && mScenario!=15) ? 1U : 0U));
    assert(mCloses==(mScenario==6 ? 0U : mScenario==4 ? 1U : 2U));
    assert(mOpens==(mScenario==6 ? 0U : 1U));
    assert(mReads==((mScenario==4 || mScenario==6) ? 0U : 1U));
    assert(mPaths==(mScenario>=7 && mScenario<14 ? 1U : 0U));
    assert(mLoads==(mScenario>=7 && mScenario<14 && mScenario!=10 ? 1U : 0U));
    assert(mStarts==(mScenario==7 || mScenario==13 ? 1U : 0U));
    assert(mUnloads==(mScenario==9 ? 1U : 0U));
  }
  puts("SD EFI: 16 host scenarios, other storage excluded, invalid PE rejected, files closed before SD-path LoadImage, security denial never started, application return not unloaded twice; no physical Windows execution.");
  return 0;
}
