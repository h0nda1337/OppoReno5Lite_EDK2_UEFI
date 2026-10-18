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

STATIC BOOLEAN Scalar (CONST OPPO_FDT_VIEW *V, CONST CHAR8 *Path,
                       CONST CHAR8 *Key, UINT32 Expected, UINT32 Width, BOOLEAN Optional)
{
  CONST VOID *Value; UINT32 Bytes, Number = 0;
  EFI_STATUS Status = OppoFdtProperty (V, Path, Key, &Value, &Bytes);
  if (Status == EFI_NOT_FOUND) return Optional;
  if (Status != EFI_SUCCESS || Bytes != Width) return FALSE;
  for (UINT32 I = 0; I < Bytes; I++) Number = (Number << 8) | ((CONST UINT8 *)Value)[I];
  return Number == Expected;
}
STATIC BOOLEAN Supply (CONST OPPO_FDT_VIEW *V, CONST CHAR8 *Key, CONST CHAR8 *Ldo)
{
  CONST VOID *A, *B; UINT32 Na, Nb;
  if (OppoFdtProperty (V, "/soc/msdc@11240000", Key, &A, &Na) != EFI_SUCCESS || Na != 4 ||
      OppoFdtProperty (V, Ldo, "phandle", &B, &Nb) != EFI_SUCCESS || Nb != 4) return FALSE;
  for (UINT32 I = 0; I < 4; I++) if (((CONST UINT8 *)A)[I] != ((CONST UINT8 *)B)[I]) return FALSE;
  return ((CONST UINT8 *)A)[0] || ((CONST UINT8 *)A)[1] || ((CONST UINT8 *)A)[2] || ((CONST UINT8 *)A)[3];
}
BOOLEAN EFIAPI OppoSdPowerProfileMatches (CONST OPPO_FDT_VIEW *V)
{
  CONST CHAR8 *Bus = "/soc/i2c5@11016000";
  CONST CHAR8 *Common = "/soc/i2c_common";
  CONST CHAR8 *Ldo = "/soc/i2c5@11016000/subpmic_ldo@64";
  CONST VOID *Value; UINT32 Bytes;
  if (!OppoSdProfileMatches (V) ||
      !Region (V, Bus, "mediatek,i2c", 0x11016000, 0x1000, 0) ||
      !Compatible (V, Common, "mediatek,i2c_common") ||
      !Compatible (V, Ldo, "mediatek,subpmic_ldo") ||
      !Scalar (V, Bus, "id", 5, 4, FALSE) ||
      !Scalar (V, Bus, "clock-div", 5, 4, FALSE) ||
      !Scalar (V, Bus, "scl-gpio-id", 28, 4, FALSE) ||
      !Scalar (V, Bus, "sda-gpio-id", 29, 4, FALSE) ||
      !Scalar (V, Bus, "mediatek,use-push-pull", 0, 0, FALSE) ||
      !Scalar (V, Common, "ver", 2, 1, FALSE) ||
      !Scalar (V, Common, "set_dt_div", 1, 1, FALSE) ||
      !Scalar (V, Common, "set_ltiming", 1, 1, FALSE) ||
      !Scalar (V, Common, "cnt_constraint", 1, 1, FALSE) ||
      !Scalar (V, Common, "fifo_support", 0, 1, TRUE) ||
      !Scalar (V, Common, "control_irq_sel", 0, 1, TRUE) ||
      !Scalar (V, Ldo, "reg", 0x64, 4, FALSE) ||
      !Supply (V, "vmmc-supply", "/soc/i2c5@11016000/subpmic_ldo@64/ldo5") ||
      !Supply (V, "vqmmc-supply", "/soc/i2c5@11016000/subpmic_ldo@64/ldo3")) return FALSE;
  // An explicit disabled bus must not become a firmware diagnostic target.
  EFI_STATUS Status = OppoFdtProperty (V, Bus, "status", &Value, &Bytes);
  return Status == EFI_NOT_FOUND || (Status == EFI_SUCCESS &&
    ((Bytes == 5 && ((CONST UINT8 *)Value)[4] == 0 && Equals (Value, 4, "okay")) ||
     (Bytes == 3 && ((CONST UINT8 *)Value)[2] == 0 && Equals (Value, 2, "ok"))));
}

BOOLEAN EFIAPI OppoSdPowerEnableProfileMatches (CONST OPPO_FDT_VIEW *V)
{
  CONST CHAR8 *Vmc = "/soc/i2c5@11016000/subpmic_ldo@64/ldo3";
  CONST CHAR8 *Vmch = "/soc/i2c5@11016000/subpmic_ldo@64/ldo5";
  return OppoSdPowerProfileMatches (V) &&
    Scalar (V, Vmc, "regulator-min-microvolt", 1200000, 4, FALSE) &&
    Scalar (V, Vmc, "regulator-max-microvolt", 3600000, 4, FALSE) &&
    Scalar (V, Vmch, "regulator-min-microvolt", 2700000, 4, FALSE) &&
    Scalar (V, Vmch, "regulator-max-microvolt", 3600000, 4, FALSE);
}
