/* SPDX-License-Identifier: BSD-2-Clause */
/* Original BDS with synthetic services; no hardware, timer IRQ or real Shell. */
#include <PiDxe.h>
#include <assert.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define _PCD_GET_MODE_BOOL_PcdNativeSdReadLab FALSE
#include "../OppoReno5LitePkg/Drivers/OppoBdsDxe/OppoBdsDxe.c"

EFI_GUID gEfiBdsArchProtocolGuid, gEfiGraphicsOutputProtocolGuid;
EFI_GUID gEfiSimpleTextOutProtocolGuid, gEfiFirmwareVolume2ProtocolGuid;
EFI_GUID gEfiHiiDatabaseProtocolGuid, gEfiHiiFontProtocolGuid, gEfiHiiStringProtocolGuid;
STATIC EFI_BOOT_SERVICES mBoot;
STATIC EFI_RUNTIME_SERVICES mRuntime;
STATIC EFI_SYSTEM_TABLE mSystem;
EFI_BOOT_SERVICES *gBS=&mBoot;
EFI_RUNTIME_SERVICES *gRT=&mRuntime;
EFI_SYSTEM_TABLE *gST=&mSystem;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL mGop;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mMode;
STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION mInfo;
STATIC EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL mOut;
STATIC EFI_SIMPLE_TEXT_OUTPUT_MODE mTextMode;
STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL mIn;
STATIC EFI_FIRMWARE_VOLUME2_PROTOCOL mVolume;
STATIC EFI_DEVICE_PATH_PROTOCOL mPath;
STATIC jmp_buf mReturn;
STATIC EFI_EVENT_NOTIFY mNotify;
STATIC UINT64 mCounter;
STATIC UINTN mScenario, mAllocated, mFreed, mStarts, mUnloads, mClosed;
STATIC BOOLEAN mTimerAttempted, mSetTimer, mApplicationAlive, mEventFired;
STATIC UINTN mModeSets, mClears;
STATIC BOOLEAN ExpectShellStart(VOID) { return mScenario<5 || mScenario==12; }

BOOLEAN EFIAPI DebugPrintEnabled(VOID) { return TRUE; }
BOOLEAN EFIAPI DebugPrintLevelEnabled(UINTN Level) { (VOID)Level;return TRUE; }
VOID EFIAPI DebugPrint(UINTN Level, CONST CHAR8 *Format, ...) { (VOID)Level;(VOID)Format; }
BOOLEAN EFIAPI ArmGetInterruptState(VOID) { return TRUE; }
UINT64 EFIAPI GetPerformanceCounter(VOID) { return mCounter; }
UINT64 EFIAPI GetPerformanceCounterProperties(UINT64 *First, UINT64 *Last)
{
  *First=mScenario==4?MAX_UINT64:0;*Last=mScenario==4?0:MAX_UINT64;return 13000000;
}
STATIC VOID *Allocate(UINTN Bytes) { VOID *p=malloc(Bytes);assert(p);mAllocated++;return p; }
VOID EFIAPI FreePool(VOID *p) { assert(p);mFreed++;free(p); }
UINT16 EFIAPI SetDevicePathNodeLength(VOID *Node, UINTN Length)
{
  assert(Node && Length<=MAX_UINT16);EFI_DEVICE_PATH_PROTOCOL *Path=Node;
  Path->Length[0]=(UINT8)Length;Path->Length[1]=(UINT8)(Length>>8);return (UINT16)Length;
}
EFI_DEVICE_PATH_PROTOCOL *EFIAPI DevicePathFromHandle(EFI_HANDLE Handle)
{ assert(Handle==(EFI_HANDLE)(UINTN)2);return &mPath; }
EFI_DEVICE_PATH_PROTOCOL *EFIAPI AppendDevicePathNode(CONST EFI_DEVICE_PATH_PROTOCOL *Path,
                                                    CONST EFI_DEVICE_PATH_PROTOCOL *Node)
{
  assert(Path==&mPath && Node->Type==MEDIA_DEVICE_PATH && Node->SubType==MEDIA_PIWG_FW_FILE_DP);
  EFI_DEVICE_PATH_PROTOCOL *copy=Allocate(sizeof(MEDIA_FW_VOL_FILEPATH_DEVICE_PATH)+4);
  memcpy(copy,Node,sizeof(MEDIA_FW_VOL_FILEPATH_DEVICE_PATH));return copy;
}
STATIC EFI_STATUS EFIAPI Locate(EFI_GUID *Guid, VOID *Registration, VOID **Interface)
{
  assert(Registration==NULL);
  if(Guid==&gEfiGraphicsOutputProtocolGuid)*Interface=&mGop;
  else if(Guid==&gEfiHiiDatabaseProtocolGuid || Guid==&gEfiHiiFontProtocolGuid || Guid==&gEfiHiiStringProtocolGuid)
    *Interface=(VOID *)(UINTN)4;
  else return EFI_NOT_FOUND;
  return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI LocateHandles(EFI_LOCATE_SEARCH_TYPE Kind, EFI_GUID *Guid,
                                      VOID *Key, UINTN *Count, EFI_HANDLE **Handles)
{
  assert(Key==NULL);*Count=Kind==AllHandles?2:1;*Handles=Allocate(*Count*sizeof(EFI_HANDLE));
  if(Kind==AllHandles) { (*Handles)[0]=(EFI_HANDLE)(UINTN)1;(*Handles)[1]=(EFI_HANDLE)(UINTN)2; }
  else { assert(Guid==&gEfiGraphicsOutputProtocolGuid || Guid==&gEfiFirmwareVolume2ProtocolGuid);
    (*Handles)[0]=(EFI_HANDLE)(UINTN)(Guid==&gEfiGraphicsOutputProtocolGuid?1:2); }
  return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI HandleProtocol(EFI_HANDLE Handle, EFI_GUID *Guid, VOID **Interface)
{
  if(Handle==(EFI_HANDLE)(UINTN)1 && Guid==&gEfiSimpleTextOutProtocolGuid)*Interface=&mOut;
  else if(Handle==(EFI_HANDLE)(UINTN)2 && Guid==&gEfiFirmwareVolume2ProtocolGuid)*Interface=&mVolume;
  else return EFI_NOT_FOUND;
  return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Connect(EFI_HANDLE Handle, EFI_HANDLE *Drivers,
                                 EFI_DEVICE_PATH_PROTOCOL *Path, BOOLEAN Recursive)
{ assert(mTimerAttempted && Handle && Drivers==NULL && Path==NULL && Recursive);return EFI_SUCCESS; }
STATIC EFI_STATUS EFIAPI Create(UINT32 Type, EFI_TPL Tpl, EFI_EVENT_NOTIFY Notify,
                               VOID *Context, EFI_EVENT *Event)
{
  assert(Type==(EVT_TIMER|EVT_NOTIFY_SIGNAL) && Tpl==TPL_CALLBACK && Context==NULL);
  mTimerAttempted=TRUE;if(mScenario==1 || mScenario==3)return EFI_OUT_OF_RESOURCES;
  *Event=(EFI_EVENT)(UINTN)5;mNotify=Notify;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI SetTimer(EFI_EVENT Event, EFI_TIMER_DELAY Type, UINT64 Ticks)
{
  assert(Event==(EFI_EVENT)(UINTN)5 && Type==TimerRelative && Ticks==900000000);
  if(mScenario==2)return EFI_DEVICE_ERROR;
  mSetTimer=TRUE;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Close(EFI_EVENT Event)
{ assert(Event==(EFI_EVENT)(UINTN)5);mClosed++;return EFI_SUCCESS; }
STATIC EFI_STATUS EFIAPI Watchdog(UINTN Seconds, UINT64 Code, UINTN Bytes, CHAR16 *Data)
{ assert(Seconds==0 && Code==0 && Bytes==0 && Data==NULL);return EFI_SUCCESS; }
STATIC EFI_STATUS EFIAPI Clear(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This)
{
  assert(This==&mOut && mModeSets==1 && This->Mode->Mode==2);
  mClears++;return mScenario==6?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Query(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Mode,
                              UINTN *Columns, UINTN *Rows)
{
  assert(This==&mOut && Mode<5);
  if(mScenario==10 || Mode==3)return EFI_UNSUPPORTED;
  if(mScenario==11) {*Columns=MAX_UINTN;*Rows=MAX_UINTN;return EFI_SUCCESS;}
  *Columns=Mode==2?135:80;*Rows=Mode==2?126:Mode==1?50:25;
  if(Mode==4)*Rows=0;
  return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI SetMode(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Mode)
{
  assert(This==&mOut && Mode==2 && mClears==0);
  assert(This->Mode->Mode==(mScenario==12?2:-1));
  mModeSets++;
  if(mScenario==5)return EFI_DEVICE_ERROR;
  if(mScenario==13)return EFI_SUCCESS; /* Broken producer did not activate it. */
  This->Mode->Mode=(INT32)Mode;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Output(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *Text)
{ assert(This==&mOut && Text);return EFI_SUCCESS; }
STATIC EFI_STATUS EFIAPI Blt(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixel,
 EFI_GRAPHICS_OUTPUT_BLT_OPERATION Operation, UINTN X, UINTN Y, UINTN Dx, UINTN Dy, UINTN W, UINTN H, UINTN Delta)
{
  assert(This==&mGop && Pixel->Red==255 && Pixel->Green==255 && Pixel->Blue==0);
  assert(Operation==EfiBltVideoFill && X==0 && Y==0 && Dx==0 && Dy==256 && W==1080 && H==24 && Delta==0);
  return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Crc(VOID *Data, UINTN Bytes, UINT32 *Result)
{ assert(Data==gST && Bytes==sizeof(mSystem));*Result=0x1234;return EFI_SUCCESS; }
STATIC EFI_STATUS EFIAPI ReadSection(CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This, CONST EFI_GUID *File,
 EFI_SECTION_TYPE Type, UINTN Instance, VOID **Buffer, UINTN *Bytes, UINT32 *Authentication)
{
  assert(This==&mVolume && memcmp(File,&mShellFile,sizeof(*File))==0 && Type==EFI_SECTION_PE32 && Instance==0);
  *Buffer=Allocate(16);*Bytes=16;*Authentication=0;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Load(BOOLEAN Policy, EFI_HANDLE Parent, EFI_DEVICE_PATH_PROTOCOL *Path,
                              VOID *Source, UINTN Bytes, EFI_HANDLE *Image)
{
  assert(!Policy && Parent==(EFI_HANDLE)(UINTN)0x100 && Path && Source && Bytes==16);
  assert(mModeSets==1 && mClears==1 && gST->ConOut==&mOut && mTextMode.Mode==2);
  *Image=(EFI_HANDLE)(UINTN)3;mApplicationAlive=TRUE;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Start(EFI_HANDLE Image, UINTN *Bytes, CHAR16 **Data)
{
  assert(Image==(EFI_HANDLE)(UINTN)3 && mApplicationAlive && Bytes==NULL && Data==NULL);
  mStarts++;mApplicationAlive=FALSE; /* Match the generic core's application lifecycle. */
  return EFI_UNSUPPORTED;
}
STATIC EFI_STATUS EFIAPI Unload(EFI_HANDLE Image)
{ (VOID)Image;mUnloads++;assert(mApplicationAlive);return EFI_SUCCESS; }
STATIC VOID EFIAPI Reset(EFI_RESET_TYPE Type, EFI_STATUS Status, UINTN Bytes, VOID *Data)
{
  assert(Type==EfiResetCold && Status==EFI_SUCCESS && Bytes==0 && Data==NULL);
  assert(mStarts==(ExpectShellStart()?1U:0U) && mUnloads==0 && mFreed==mAllocated);
  longjmp(mReturn,1);
}
STATIC EFI_STATUS EFIAPI Stall(UINTN Microseconds)
{
  assert(Microseconds==100000);
  if(mScenario==0 || mScenario==4) { assert(mSetTimer && mNotify);mEventFired=TRUE;mNotify((EFI_EVENT)(UINTN)5,NULL); }
  mCounter=mCounterStart+mCounterTicks;return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Install(EFI_HANDLE *Handle, ...)
{
  va_list Args;va_start(Args,Handle);assert(va_arg(Args,EFI_GUID *)==&gEfiBdsArchProtocolGuid);
  assert(va_arg(Args,VOID *)==&mBds && va_arg(Args,VOID *)==NULL);va_end(Args);
  *Handle=(EFI_HANDLE)(UINTN)6;return EFI_SUCCESS;
}
int main(void)
{
  mBoot.LocateProtocol=Locate;mBoot.LocateHandleBuffer=LocateHandles;mBoot.HandleProtocol=HandleProtocol;
  mBoot.ConnectController=Connect;mBoot.CreateEvent=Create;mBoot.SetTimer=SetTimer;mBoot.CloseEvent=Close;
  mBoot.SetWatchdogTimer=Watchdog;mBoot.CalculateCrc32=Crc;mBoot.LoadImage=Load;mBoot.StartImage=Start;
  mBoot.UnloadImage=Unload;mBoot.Stall=Stall;mBoot.InstallMultipleProtocolInterfaces=Install;
  mRuntime.ResetSystem=Reset;mGop.Mode=&mMode;mGop.Blt=Blt;mMode.Info=&mInfo;
  mInfo.HorizontalResolution=1080;mInfo.VerticalResolution=2400;
  mOut.ClearScreen=Clear;mOut.OutputString=Output;mOut.QueryMode=Query;mOut.SetMode=SetMode;
  mVolume.ReadSection=ReadSection;
  for(mScenario=0;mScenario<14;mScenario++) {
    mAllocated=mFreed=mStarts=mUnloads=mClosed=0;
    mModeSets=mClears=0;mOut.Mode=&mTextMode;mTextMode.Mode=-1;mTextMode.MaxMode=5;
    if(mScenario==7)mTextMode.MaxMode=0;
    if(mScenario==8)mOut.Mode=NULL;
    if(mScenario==9)mTextMode.MaxMode=129;
    if(mScenario==12)mTextMode.Mode=2; /* Preferred index, rendering not initialized. */
    mTimerAttempted=mSetTimer=mApplicationAlive=mEventFired=FALSE;mNotify=NULL;
    mCounter=mScenario==3?MAX_UINT64-10:100;mSystem.ConIn=&mIn;mSystem.ConOut=NULL;mSystem.StdErr=NULL;
    mSystem.Hdr.HeaderSize=sizeof(mSystem);
    assert(OppoBdsEntry((EFI_HANDLE)(UINTN)0x100,&mSystem)==EFI_SUCCESS);
    if(setjmp(mReturn)==0)mBds.Entry(&mBds);
    assert(mTimerAttempted && mStarts==(ExpectShellStart()?1U:0U) && mUnloads==0 && mFreed==mAllocated);
    assert((mSystem.ConOut==&mOut)==ExpectShellStart());
    assert(mClears==(ExpectShellStart() || mScenario==6?1U:0U));
    assert(mModeSets==(mScenario<=6 || mScenario>=12?1U:0U));
    assert(mEventFired==(mScenario==0 || mScenario==4));
    assert(mClosed==(mScenario==2?1U:0U));
    assert(mCounterDeadlineReady==(mScenario!=4) && mSystem.Hdr.CRC32==0x1234);
  }
  puts("Original BDS: 14 scenarios passed host sanitizers, including explicit activation of a preferred index and rejection of a producer that fails to commit SetMode; no hardware test.");
  return 0;
}
