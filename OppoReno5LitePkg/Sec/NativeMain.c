/** @file
  Original OPPO SEC startup using generic EDK2 core services.
  Experimental: build/inspection only until a physical session validates it.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <PiPei.h>
#include <Pi/PiBootMode.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Guid/OppoBootInfo.h>
#include <OppoSecEntry.h>
#include <OppoNativeProfile.h>
#include <Library/ArmLib.h>
#include <Library/ArmMmuLib.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/OppoDebugLog.h>
#include <Library/HobLib.h>
#include <Library/PrePiLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/OppoFdtLib.h>
#include <Library/OppoFrameBufferLib.h>
#include <Library/OppoMemoryPlanLib.h>

VOID EFIAPI ProcessLibraryConstructorList (VOID);

typedef struct {
  CONST OPPO_SEC_ENTRY_CONTEXT *Entry;
  OPPO_BOOT_HANDOFF Handoff;
  OPPO_EARLY_MEMORY_PLAN Plan;
  UINT32 PsciVersion;
  BOOLEAN SdProfile;
} BOOT_STATE;

STATIC BOOLEAN mResetReady;
STATIC BOOLEAN mMarkerReady;
STATIC OPPO_FB_CONTEXT mMarker;

STATIC UINT64 BigNumber (CONST UINT8 *P, UINTN Bytes)
{
  UINT64 Value = 0;
  for (UINTN I = 0; I < Bytes; I++) Value = (Value << 8) | P[I];
  return Value;
}

STATIC VOID Stage (UINTN Number, EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color)
{
  if (mMarkerReady)
    OppoFbBlt (&mMarker, &Color, sizeof (Color), EfiBltVideoFill,
               0, 0, 0, 160 + Number * 32, OPPO_NATIVE_FB_WIDTH, 24, 0);
}

STATIC VOID Stop (VOID)
{
  Stage (3, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0, 0, 255, 0});
  if (mResetReady) ArmCallSmc0 (ARM_SMC_ID_PSCI_SYSTEM_RESET, NULL, NULL, NULL);
  CpuDeadLoop ();
}

STATIC VOID EntryProbe (CONST OPPO_SEC_ENTRY_CONTEXT *Entry)
{
  // Each build retains only its selected marker. Packaging rejects other modes.
  if (FeaturePcdGet (PcdNativeMmuProbe)) {
    STATIC CONST volatile CHAR8 Identity[] = "OPPO_SEC_MMU_PROBE_ONLY_V1";
    if (Identity[0] != 'O') Stop ();
  } else {
    STATIC CONST volatile CHAR8 Identity[] = "OPPO_SEC_PROBE_ONLY_V1";
    if (Identity[0] != 'O') Stop ();
  }
  if (Entry->CounterFrequency > 1000000000ULL) Stop ();
  Stage (FeaturePcdGet (PcdNativeMmuProbe) ? 2 : 1,
         (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){255, 255, 255, 0});
  UINT64 Start = ArmReadCntPct ();
  UINT64 Ticks = Entry->CounterFrequency * 10;
  while (ArmReadCntPct () - Start < Ticks) CpuPause ();
  // This request is a test of PSCI, not a proven fallback for a CPU exception.
  ArmCallSmc0 (ARM_SMC_ID_PSCI_SYSTEM_RESET, NULL, NULL, NULL);
  Stop ();
  for (;;) CpuDeadLoop ();
}

STATIC BOOLEAN RegisterMatches (CONST OPPO_FDT_VIEW *View, CONST CHAR8 *Path,
                                UINT64 Base, UINT64 MinimumSize, UINTN Offset)
{
  CONST VOID *Value;
  UINT32 Bytes;
  if (OppoFdtProperty (View, Path, "reg", &Value, &Bytes) != EFI_SUCCESS ||
      Offset > Bytes || Bytes - Offset < 16) return FALSE;
  CONST UINT8 *Reg = (CONST UINT8 *)Value + Offset;
  return (BOOLEAN)(BigNumber (Reg, 8) == Base && BigNumber (Reg + 8, 8) >= MinimumSize);
}

STATIC VOID MapRegion (ARM_MEMORY_REGION_DESCRIPTOR *Map, UINTN *Count,
                       OPPO_MEMORY_RANGE R, ARM_MEMORY_REGION_ATTRIBUTES Attributes)
{
  Map[*Count].PhysicalBase = R.Base;
  Map[*Count].VirtualBase = R.Base;
  Map[*Count].Length = R.Length;
  Map[*Count].Attributes = Attributes;
  (*Count)++;
}

STATIC VOID EFIAPI ContinueBoot (VOID *Context, VOID *Unused)
{
  BOOT_STATE *State = Context;
  CONST OPPO_EARLY_MEMORY_PLAN *Plan = &State->Plan;
  UINT64 StackBase = Plan->Pool.Base + Plan->Pool.Length - OPPO_NATIVE_STACK_BYTES;
  EFI_HOB_HANDOFF_INFO_TABLE *Hobs;
  EFI_RESOURCE_ATTRIBUTE_TYPE Attributes = EFI_RESOURCE_ATTRIBUTE_PRESENT |
    EFI_RESOURCE_ATTRIBUTE_INITIALIZED | EFI_RESOURCE_ATTRIBUTE_TESTED |
    EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE | EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE;
  ARM_MEMORY_REGION_DESCRIPTOR Map[OPPO_MAX_BOOT_RESERVATIONS + 12];
  UINTN MapCount = 0, Index;
  EFI_STATUS Status;
  OPPO_BOOT_INFO Info;
  (VOID)Unused;
  Hobs = HobConstructor ((VOID *)(UINTN)Plan->Pool.Base, (UINTN)Plan->Pool.Length,
                         (VOID *)(UINTN)Plan->Pool.Base, (VOID *)(UINTN)OPPO_NATIVE_DEBUG_BASE);
  if (Hobs == NULL || PrePeiSetHobList (Hobs) != EFI_SUCCESS) Stop ();
  BuildResourceDescriptorHob (EFI_RESOURCE_SYSTEM_MEMORY, Attributes, Plan->Pool.Base, Plan->Pool.Length);
  for (Index = 0; Index < Plan->ProtectedCount; Index++) {
    OPPO_MEMORY_RANGE R = Plan->Protected[Index];
    EFI_MEMORY_TYPE Type = Plan->ProtectedType[Index];
    BuildResourceDescriptorHob (Type == EfiReservedMemoryType ? EFI_RESOURCE_MEMORY_RESERVED :
                                EFI_RESOURCE_SYSTEM_MEMORY, Attributes, R.Base, R.Length);
    if (Type != EfiReservedMemoryType) BuildMemoryAllocationHob (R.Base, R.Length, Type);
  }
  BuildStackHob (StackBase, OPPO_NATIVE_STACK_BYTES);
  BuildMemoryAllocationHob (OPPO_NATIVE_DEBUG_BASE, 4096, EfiBootServicesData);
  BuildCpuHob ((UINT8)ArmGetPhysicalAddressBits (), 0);
  SetBootMode (BOOT_WITH_FULL_CONFIGURATION);
  BuildFvHob (OPPO_NATIVE_FV_BASE, OPPO_NATIVE_FV_BYTES);
  ProcessLibraryConstructorList ();
  ZeroMem (Map, sizeof (Map));
  MapRegion (Map, &MapCount, Plan->Image, ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK);
  MapRegion (Map, &MapCount, Plan->Pool, ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK);
  for (Index = 0; Index < Plan->ProtectedCount; Index++)
    if (Plan->ProtectedType[Index] == EfiBootServicesData)
      MapRegion (Map, &MapCount, Plan->Protected[Index], ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK);
  MapRegion (Map, &MapCount, State->Handoff.NativeFramebuffer, ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED);
  MapRegion (Map, &MapCount, (OPPO_MEMORY_RANGE){OPPO_NATIVE_GICD_BASE, 0x40000}, ARM_MEMORY_REGION_ATTRIBUTE_DEVICE);
  MapRegion (Map, &MapCount, (OPPO_MEMORY_RANGE){OPPO_NATIVE_GICR_BASE, 0x200000}, ARM_MEMORY_REGION_ATTRIBUTE_DEVICE);
  MapRegion (Map, &MapCount, (OPPO_MEMORY_RANGE){OPPO_NATIVE_WDT_BASE, 0x1000}, ARM_MEMORY_REGION_ATTRIBUTE_DEVICE);
  if (FeaturePcdGet (PcdNativeSdReadLab) && State->SdProfile) {
    MapRegion (Map, &MapCount, (OPPO_MEMORY_RANGE){0x11240000, 0x10000}, ARM_MEMORY_REGION_ATTRIBUTE_DEVICE);
    MapRegion (Map, &MapCount, (OPPO_MEMORY_RANGE){0x10000000, 0x1000}, ARM_MEMORY_REGION_ATTRIBUTE_DEVICE);
    MapRegion (Map, &MapCount, (OPPO_MEMORY_RANGE){0x10001000, 0x1000}, ARM_MEMORY_REGION_ATTRIBUTE_DEVICE);
    MapRegion (Map, &MapCount, (OPPO_MEMORY_RANGE){0x10005000, 0x1000}, ARM_MEMORY_REGION_ATTRIBUTE_DEVICE);
    MapRegion (Map, &MapCount, (OPPO_MEMORY_RANGE){0x11e70000, 0x1000}, ARM_MEMORY_REGION_ATTRIBUTE_DEVICE);
  }
  if (State->Entry->ExceptionLevel == 2) {
    ArmWriteHcr ((1ULL << 31) | (1ULL << 27));
    ArmWriteCntHctl (3);
  }
  Status = ArmConfigureMmu (Map, NULL, NULL);
  if (Status != EFI_SUCCESS) Stop ();
  ArmEnableInstructionCache (); ArmEnableDataCache ();
  if ((ArmReadSctlr () & 0x1005U) != 0x1005U) Stop ();
  Stage (1, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){255, 0, 0, 0});
  // Observe HOB construction, stack switch and MMU before attempting DXE.
  if (FeaturePcdGet (PcdNativeMmuProbe)) EntryProbe (State->Entry);
  ZeroMem (&Info, sizeof (Info));
  Info.Signature = OPPO_BOOT_INFO_SIGNATURE; Info.Revision = OPPO_BOOT_INFO_REVISION;
  Info.InitialEl = State->Entry->ExceptionLevel; Info.CounterFrequency = State->Entry->CounterFrequency;
  Info.FramebufferBase = State->Handoff.NativeFramebuffer.Base;
  Info.FramebufferBytes = State->Handoff.NativeFramebuffer.Length;
  Info.Width = OPPO_NATIVE_FB_WIDTH; Info.Height = OPPO_NATIVE_FB_HEIGHT; Info.Stride = OPPO_NATIVE_FB_STRIDE;
  Info.RedShift = OPPO_NATIVE_FB_RED_SHIFT;
  Info.BlueShift = OPPO_NATIVE_FB_BLUE_SHIFT; Info.OpaqueAlpha = TRUE;
  Info.InitialSctlr = State->Entry->Sctlr; Info.PsciVersion = State->PsciVersion;
  Info.Flags = OPPO_BOOT_MEMORY_READY | OPPO_BOOT_EXPECTED_DISPLAY_PROFILE | OPPO_BOOT_WATCHDOG_STOPPED;
  if (State->SdProfile) Info.Flags |= OPPO_BOOT_SD_PROFILE_VALIDATED | OPPO_BOOT_SD_PAD_BANK_VALIDATED;
  if (BuildGuidDataHob (&gOppoBootInfoHobGuid, &Info, sizeof (Info)) == NULL) Stop ();
  Stage (2, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){255, 255, 255, 0});
  LoadDxeCoreFromFv (NULL, 0);
  Stop ();
}

VOID EFIAPI OppoSecMain (IN CONST OPPO_SEC_ENTRY_CONTEXT *Entry)
{
  // Linked core routines may use SIMD even before the memory map exists.
  ArmEnableVFP ();
  BOOT_STATE State;
  OPPO_FDT_VIEW View;
  CONST VOID *Value;
  UINT32 Bytes;
  ARM_SMC_ARGS Args;
  UINT64 Fdt = Entry->Fdt;
  EFI_STATUS Status;
  if (FeaturePcdGet (PcdNativeEntryProbe) && FeaturePcdGet (PcdNativeMmuProbe)) Stop ();
  if ((Entry->ExceptionLevel != 1 && Entry->ExceptionLevel != 2) || Entry->CounterFrequency == 0 ||
      Fdt < OPPO_NATIVE_DRAM_BASE || Fdt > OPPO_NATIVE_DRAM_END - 40 || (Fdt & 7)) Stop ();
  UINT64 Total = BigNumber ((CONST UINT8 *)(UINTN)Fdt + 4, 4);
  if (Total > 2 * 1024 * 1024 || Total > OPPO_NATIVE_DRAM_END - Fdt ||
      !OppoFdtOpen ((CONST VOID *)(UINTN)Fdt, (UINTN)Total, &View)) Stop ();
  if (OppoFdtProperty (&View, "/", "compatible", &Value, &Bytes) != EFI_SUCCESS ||
      Bytes != sizeof ("mediatek,MT6779") || CompareMem (Value, "mediatek,MT6779", Bytes)) Stop ();
  // Validate the declared PSCI binding before making the first SMC call.
  if (OppoFdtProperty (&View, "/psci", "compatible", &Value, &Bytes) != EFI_SUCCESS ||
      Bytes != sizeof ("arm,psci-1.0") ||
      (CompareMem (Value, "arm,psci-1.0", Bytes) && CompareMem (Value, "arm,psci-0.2", Bytes))) Stop ();
  if (OppoFdtProperty (&View, "/psci", "method", &Value, &Bytes) != EFI_SUCCESS ||
      Bytes != 4 || CompareMem (Value, "smc", 4)) Stop ();
  ZeroMem (&Args, sizeof (Args)); Args.Arg0 = ARM_SMC_ID_PSCI_VERSION; ArmCallSmc (&Args);
  State.PsciVersion = (UINT32)Args.Arg0;
  if ((State.PsciVersion & 0x80000000U) || State.PsciVersion < 2) Stop ();
  mResetReady = TRUE;
  State.SdProfile = FALSE;
  // No storage MMIO is mapped unless this boot's FDT matches the board profile.
  if (FeaturePcdGet (PcdNativeSdReadLab)) {
    State.SdProfile = RegisterMatches (&View, "/soc/msdc@11240000", 0x11240000, 0x10000, 0) &&
      RegisterMatches (&View, "/soc/clock-controller@10000000", 0x10000000, 0x1000, 0) &&
      RegisterMatches (&View, "/soc/clock-reset-controller@10001000", 0x10001000, 0x1000, 0) &&
      RegisterMatches (&View, "/soc/pinctrl@10005000", 0x10005000, 0x1000, 0) &&
      RegisterMatches (&View, "/soc/pinctrl@10005000", 0x11e70000, 0x1000, 64) &&
      OppoFdtProperty (&View, "/soc/msdc@11240000", "host_function", &Value, &Bytes) == EFI_SUCCESS &&
      Bytes == 1 && *(CONST UINT8 *)Value == 1;
  }
  if (!RegisterMatches (&View, "/soc/toprgu@10007000", OPPO_NATIVE_WDT_BASE, 0x1000, 0) ||
      !RegisterMatches (&View, "/soc/interrupt-controller@0c000000", OPPO_NATIVE_GICD_BASE, 0x40000, 0) ||
      !RegisterMatches (&View, "/soc/interrupt-controller@0c000000", OPPO_NATIVE_GICR_BASE, 0x200000, 16)) Stop ();
  // MT6779 mode key and enable bit, checked against the public OPPO reference.
  volatile UINT32 *Mode = (volatile UINT32 *)(UINTN)OPPO_NATIVE_WDT_BASE;
  *Mode = ((*Mode & 0x00ffffffU) & ~1U) | 0x22000000U;
  MemoryFence ();
  if (*Mode & 1U) Stop ();
  State.Entry = Entry;
  Status = OppoInspectHandoff ((CONST VOID *)(UINTN)Fdt, (UINTN)Total, Fdt, &State.Handoff);
  if (Status != EFI_SUCCESS) Stop ();
  Status = OppoPlanEarlyMemory (&State.Handoff,
    (OPPO_MEMORY_RANGE){OPPO_NATIVE_FD_BASE, OPPO_NATIVE_FD_BYTES},
    (OPPO_MEMORY_RANGE){OPPO_NATIVE_POOL_BASE, OPPO_NATIVE_POOL_BYTES}, &State.Plan);
  if (Status != EFI_SUCCESS || !State.Handoff.NativePanelFound ||
      !State.Handoff.NativeFramebufferCoveredByReservation ||
      State.Handoff.NativeFramebuffer.Base != OPPO_NATIVE_FB_BASE) Stop ();
  OPPO_FB_LAYOUT Layout = {OPPO_NATIVE_FB_WIDTH, OPPO_NATIVE_FB_HEIGHT, OPPO_NATIVE_FB_STRIDE,
                           OPPO_NATIVE_FB_RED_SHIFT, OPPO_NATIVE_FB_BLUE_SHIFT, TRUE};
  if (!OppoFbInitialize (&mMarker, (VOID *)(UINTN)State.Handoff.NativeFramebuffer.Base,
                         (UINTN)State.Handoff.NativeFramebuffer.Length, &Layout)) Stop ();
  mMarkerReady = TRUE;
  Stage (0, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0, 255, 0, 0});
  if (!OppoDebugStart ()) Stop ();
  DEBUG ((DEBUG_INFO, "SEC handoff validated; EL %u, counter %Lu\n",
          (UINT32)Entry->ExceptionLevel, Entry->CounterFrequency));
  if (FeaturePcdGet (PcdNativeEntryProbe)) EntryProbe (Entry);
  SwitchStack (ContinueBoot, &State, NULL,
               (VOID *)(UINTN)(State.Plan.Pool.Base + State.Plan.Pool.Length));
  Stop ();
}
