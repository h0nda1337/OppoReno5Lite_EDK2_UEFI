/* SPDX-License-Identifier: BSD-2-Clause */
/* Original role-aware profile inspection; never accesses physical hardware. */
#include <Library/OppoHandoffLib.h>
#include <OppoNativeProfile.h>

STATIC BOOLEAN Equals (CONST UINT8 *P, UINTN Bytes, CONST CHAR8 *S)
{
  UINTN I = 0;
  while (S[I] != 0) { if (I >= Bytes || P[I] != (UINT8)S[I]) return FALSE; I++; }
  return I == Bytes;
}
STATIC BOOLEAN Compatible (CONST OPPO_FDT_VIEW *V, CONST CHAR8 *Path, CONST CHAR8 *Name)
{
  CONST VOID *Value; UINT32 Bytes;
  BOOLEAN Found = FALSE;
  if (OppoFdtProperty (V, Path, "compatible", &Value, &Bytes) != EFI_SUCCESS) return FALSE;
  CONST UINT8 *P = Value;
  for (UINTN Start = 0; Start < Bytes;) {
    UINTN End = Start;
    while (End < Bytes && P[End] != 0) End++;
    if (End == Bytes || End == Start) return FALSE;
    if (Equals (P + Start, End - Start, Name)) Found = TRUE;
    Start = End + 1;
  }
  return Found;
}
STATIC UINT64 Big (CONST UINT8 *P)
{
  UINT64 N = 0;
  for (UINTN I = 0; I < 8; I++) N = (N << 8) | P[I];
  return N;
}
STATIC BOOLEAN Region (CONST OPPO_FDT_VIEW *V, CONST CHAR8 *Path,
                       CONST CHAR8 *Role, UINT64 Base, UINT64 Minimum, UINTN Offset)
{
  CONST VOID *Value; UINT32 Bytes;
  if (!Compatible (V, Path, Role) ||
      OppoFdtProperty (V, Path, "reg", &Value, &Bytes) != EFI_SUCCESS ||
      Offset > Bytes || Bytes - Offset < 16) return FALSE;
  CONST UINT8 *P = (CONST UINT8 *)Value + Offset;
  return Big (P) == Base && Big (P + 8) >= Minimum;
}
BOOLEAN EFIAPI OppoSdProfileMatches (CONST OPPO_FDT_VIEW *V)
{
  CONST VOID *Value; UINT32 Bytes;
  if (V == NULL || !Compatible (V, "/", "mediatek,MT6779") ||
      !Region (V, "/soc/msdc@11240000", "mediatek,msdc", 0x11240000, 0x10000, 0) ||
      !Region (V, "/soc/clock-controller@10000000", "mediatek,mt6779-topckgen", 0x10000000, 0x1000, 0) ||
      !Region (V, "/soc/clock-reset-controller@10001000", "mediatek,mt6779-infracfg_ao", 0x10001000, 0x1000, 0) ||
      !Region (V, "/soc/pinctrl@10005000", "mediatek,mt6779-pinctrl", 0x10005000, 0x1000, 0) ||
      !Region (V, "/soc/pinctrl@10005000", "mediatek,mt6779-pinctrl", OPPO_NATIVE_SD_PAD_BASE, 0x1000, 16) ||
      !Region (V, OPPO_NATIVE_SD_PAD_PATH, "mediatek,iocfg_rm", OPPO_NATIVE_SD_PAD_BASE, 0x1000, 0)) return FALSE;
  if (OppoFdtProperty (V, "/soc/msdc@11240000", "host_function", &Value, &Bytes) != EFI_SUCCESS ||
      Bytes != 1 || *(CONST UINT8 *)Value != 1) return FALSE;
  if (OppoFdtProperty (V, "/soc/msdc@11240000", "bus-width", &Value, &Bytes) != EFI_SUCCESS ||
      Bytes != 4) return FALSE;
  CONST UINT8 *P = Value;
  if (P[0] || P[1] || P[2] || P[3] != 4) return FALSE;
  if (OppoFdtProperty (V, "/soc/msdc@11240000", "status", &Value, &Bytes) != EFI_SUCCESS) return FALSE;
  P = Value;
  return (Bytes == 5 && P[4] == 0 && Equals (P, 4, "okay")) ||
         (Bytes == 3 && P[2] == 0 && Equals (P, 2, "ok"));
}
