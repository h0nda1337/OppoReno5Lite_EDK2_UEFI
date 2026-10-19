/** Original bounded MT6779 display observer; register contracts only were researched.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include "DisplayProbe.h"

STATIC BOOLEAN Match (CONST OPPO_FDT_VIEW *View, CONST CHAR8 *Path, CONST CHAR8 *Name,
                      CONST VOID *Expected, UINT32 Size)
{
  CONST VOID *Value;
  UINT32 Bytes;
  if (OppoFdtProperty (View, Path, Name, &Value, &Bytes) != EFI_SUCCESS || Bytes != Size)
    return FALSE;
  for (UINT32 I = 0; I < Size; I++)
    if (((CONST UINT8 *)Value)[I] != ((CONST UINT8 *)Expected)[I]) return FALSE;
  return TRUE;
}

STATIC BOOLEAN Binding (CONST OPPO_FDT_VIEW *View, CONST CHAR8 *Path,
                        CONST CHAR8 *Compatible, UINT32 CompatibleBytes, UINT32 Base)
{
  CONST VOID *Status;
  UINT32 Bytes;
  UINT8 Reg[16] = {0};
  for (UINT32 I = 0; I < 4; I++) Reg[4 + I] = (UINT8)(Base >> (24 - 8*I));
  Reg[14] = 0x10; /* Exactly one 4 KiB resource with 64-bit address/size cells. */
  if (!Match (View, Path, "compatible", Compatible, CompatibleBytes) ||
      !Match (View, Path, "reg", Reg, sizeof (Reg))) return FALSE;
  EFI_STATUS Result = OppoFdtProperty (View, Path, "status", &Status, &Bytes);
  if (Result == EFI_NOT_FOUND) return TRUE;
  return Result == EFI_SUCCESS &&
    (Match (View, Path, "status", "okay", 5) || Match (View, Path, "status", "ok", 3));
}

EFI_STATUS OppoDisplayReadSnapshot (CONST OPPO_FDT_VIEW *View, OPPO_DISPLAY_READ32 Read,
                                    VOID *Context, OPPO_DISPLAY_SNAPSHOT *Output)
{
  STATIC CONST UINT8 Two[] = {0,0,0,2};
  STATIC CONST CHAR8 *Paths[] = {"/soc/disp_ovl0@14008000", "/soc/disp_ovl0_2l@14009000",
                               "/soc/disp_ovl1_2l@1400a000"};
  STATIC CONST CHAR8 *Compatible[] = {"mediatek,mt6779-disp_ovl0", "mediatek,mt6779-disp_ovl0_2l",
                                    "mediatek,mt6779-disp_ovl1_2l"};
  STATIC CONST UINT32 CompatibleBytes[] = {sizeof ("mediatek,mt6779-disp_ovl0"),
      sizeof ("mediatek,mt6779-disp_ovl0_2l"), sizeof ("mediatek,mt6779-disp_ovl1_2l")};
  STATIC CONST CHAR8 MmsysCompatible[] = "mediatek,mt6779-mmsys\0syscon";
  OPPO_DISPLAY_SNAPSHOT Candidate = {0};
  if (Output == NULL) return EFI_INVALID_PARAMETER;
  *Output = Candidate;
  if (View == NULL || Read == NULL) return EFI_INVALID_PARAMETER;
  if (!Match (View, "/", "compatible", "mediatek,MT6779", sizeof ("mediatek,MT6779")) ||
      !Match (View, "/soc", "#address-cells", Two, 4) ||
      !Match (View, "/soc", "#size-cells", Two, 4) ||
      !Match (View, "/soc", "ranges", "", 0) ||
      !Binding (View, "/soc/clock-controller@14000000", MmsysCompatible,
                sizeof (MmsysCompatible), 0x14000000))
    return EFI_UNSUPPORTED;
  for (UINT32 B = 0; B < 3; B++)
    if (!Binding (View, Paths[B], Compatible[B], CompatibleBytes[B], 0x14008000U + B*0x1000U))
      return EFI_UNSUPPORTED;
  if (Read (Context, 0x14000100, &Candidate.ClockGate) != EFI_SUCCESS) return EFI_DEVICE_ERROR;
  for (UINT32 B = 0; B < 3; B++) {
    OPPO_DISPLAY_BANK *Bank = &Candidate.Banks[B];
    Bank->Base = 0x14008000U + B*0x1000U;
    Bank->ClockOff = (Candidate.ClockGate >> (20 + B)) & 1U;
    if (Bank->ClockOff) continue;
    UINT32 GlobalOffsets[] = {0x00c, 0x02c, 0x324, 0x024, 0x020};
    UINT32 *Values[] = {&Bank->Enable, &Bank->Sources, &Bank->Extended, &Bank->DataPath, &Bank->Roi};
    for (UINT32 I = 0; I < ARRAY_SIZE (GlobalOffsets); I++)
      if (Read (Context, Bank->Base + GlobalOffsets[I], Values[I]) != EFI_SUCCESS) return EFI_DEVICE_ERROR;
    if ((Bank->Enable & 1U) == 0) continue;
    for (UINT32 L = 0; L < 7; L++) {
      OPPO_DISPLAY_LAYER *Layer = &Bank->Layers[L];
      BOOLEAN Extended = L >= 4;
      UINT32 Index = Extended ? L - 4 : L;
      if ((!Extended && Index >= (B == 0 ? 4U : 2U)) ||
          (((Extended ? Bank->Extended : Bank->Sources) >> Index) & 1U) == 0) continue;
      Layer->Enabled = 1;
      UINT32 Control = (Extended ? 0x330U : 0x030U) + Index*0x20U;
      UINT32 Address = Extended ? 0xfb0U + Index*4U : 0xf40U + Index*0x20U;
      UINT32 Offsets[] = {Control, Address, Control + 8, Control + 12, Control + 20};
      UINT32 *Fields[] = {&Layer->Control, &Layer->Address, &Layer->Size, &Layer->Offset, &Layer->Pitch};
      for (UINT32 I = 0; I < ARRAY_SIZE (Offsets); I++)
        if (Read (Context, Bank->Base + Offsets[I], Fields[I]) != EFI_SUCCESS) return EFI_DEVICE_ERROR;
    }
  }
  *Output = Candidate;
  return EFI_SUCCESS;
}

EFI_STATUS OppoDisplayHideInherited (CONST OPPO_FDT_VIEW *View, OPPO_DISPLAY_READ32 Read,
                                     OPPO_DISPLAY_WRITE32 Write, VOID *Context,
                                     OPPO_DISPLAY_CLEAN_RESULT *Result)
{
  OPPO_DISPLAY_SNAPSHOT Snapshot;
  OPPO_DISPLAY_CLEAN_RESULT Empty = {0};
  UINT32 Current;
  if (Result == NULL) return EFI_INVALID_PARAMETER;
  *Result = Empty;
  if (Read == NULL || Write == NULL) return EFI_INVALID_PARAMETER;
  EFI_STATUS Status = OppoDisplayReadSnapshot (View, Read, Context, &Snapshot);
  if (EFI_ERROR (Status)) return Status;
  CONST OPPO_DISPLAY_BANK *Overlay = &Snapshot.Banks[0], *Gop = &Snapshot.Banks[1];
  CONST OPPO_DISPLAY_LAYER *Inherited = &Overlay->Layers[1], *Main = &Gop->Layers[0];
  // Exact physical phase36 profile. Unknown routing/formats/layers are untouched.
  if (Overlay->ClockOff || Gop->ClockOff || Snapshot.Banks[2].ClockOff != 1 ||
      Overlay->Enable != 1 || Gop->Enable != 1 ||
      Overlay->Sources != 0x12 || Gop->Sources != 1 || Overlay->Extended != 0 || Gop->Extended != 0 ||
      Overlay->DataPath != 0x07000001 || Gop->DataPath != 0x07000001 ||
      Overlay->Roi != 0x09600438 || Gop->Roi != 0x09600438 ||
      !Inherited->Enabled || Inherited->Address != 0x76f16000 || Inherited->Control != 0x21ff ||
      Inherited->Pitch != 0x21ff10e0 || Inherited->Size != 0x09600438 || Inherited->Offset != 0 ||
      !Main->Enabled || Main->Address != 0x76520000 || Main->Control != 0x31ff ||
      Main->Pitch != 0x21ff1100 || Main->Size != 0x09600438 || Main->Offset != 0)
    return EFI_UNSUPPORTED;
  // Check again immediately before the one permitted register change.
  if (Read (Context, 0x1400802c, &Current) != EFI_SUCCESS) return EFI_DEVICE_ERROR;
  if (Current != Overlay->Sources) return EFI_ABORTED;
  Result->Before = Current;
  Result->Attempted = TRUE;
  Status = Write (Context, 0x1400802c, Current & ~2U);
  if (EFI_ERROR (Status)) return Status;
  if (Read (Context, 0x1400802c, &Result->After) != EFI_SUCCESS) return EFI_DEVICE_ERROR;
  return Result->After == (Current & ~2U) ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}
