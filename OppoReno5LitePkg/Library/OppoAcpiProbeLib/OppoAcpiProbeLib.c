/** @file
  Original MT6779 observation policy. Register format references: docs/ACPI.md.
  The callback has no write operation and is never called for an unknown FDT.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Library/OppoAcpiProbeLib.h>
#include <Library/OppoFdtLib.h>
#include <Library/BaseMemoryLib.h>

#define GIC_PATH "/soc/interrupt-controller@0c000000"
#define GICD_BASE 0x0c000000ULL
#define GICR_BASE 0x0c040000ULL
#define GICR_SIZE 0x200000ULL
#define GICR_STRIDE 0x20000ULL
#define PIDR2 0xffe8U
#define MPIDR_MASK 0x000000ff00ffffffULL
STATIC_ASSERT (OPPO_ACPI_CPU_COUNT == 8, "MT6779 affinity profile");

STATIC UINT32 Be32 (CONST UINT8 *P)
{ return (UINT32)P[0] << 24 | (UINT32)P[1] << 16 | (UINT32)P[2] << 8 | P[3]; }

STATIC BOOLEAN Starts (CONST CHAR8 *S, CONST CHAR8 *Prefix)
{
  for (UINTN I = 0; Prefix[I] != 0; I++) if (S[I] != Prefix[I]) return FALSE;
  return TRUE;
}

STATIC BOOLEAN Text (CONST OPPO_FDT_VIEW *V, CONST CHAR8 *Path, CONST CHAR8 *Key, CONST CHAR8 *Wanted)
{
  CONST VOID *Value;
  UINT32 Bytes;
  UINTN N = 0;
  while (Wanted[N] != 0) N++;
  return (BOOLEAN)(OppoFdtProperty (V, Path, Key, &Value, &Bytes) == EFI_SUCCESS &&
                   Bytes == N + 1 && CompareMem (Value, Wanted, Bytes) == 0);
}

STATIC BOOLEAN Numbers (CONST OPPO_FDT_VIEW *V, CONST CHAR8 *Path, CONST CHAR8 *Key,
                       CONST UINT32 *Wanted, UINTN Count)
{
  CONST VOID *Value;
  UINT32 Bytes;
  if (OppoFdtProperty (V, Path, Key, &Value, &Bytes) != EFI_SUCCESS || Bytes != Count * 4) return FALSE;
  for (UINTN I = 0; I < Count; I++) if (Be32 ((CONST UINT8 *)Value + I * 4) != Wanted[I]) return FALSE;
  return TRUE;
}

STATIC BOOLEAN Enabled (CONST OPPO_FDT_VIEW *V, CONST CHAR8 *Path)
{
  CONST VOID *Value;
  UINT32 Bytes;
  EFI_STATUS S = OppoFdtProperty (V, Path, "status", &Value, &Bytes);
  return (BOOLEAN)(S == EFI_NOT_FOUND || (S == EFI_SUCCESS &&
    ((Bytes == 5 && CompareMem (Value, "okay", 5) == 0) ||
     (Bytes == 3 && CompareMem (Value, "ok", 3) == 0))));
}

typedef struct { CONST OPPO_FDT_VIEW *V; UINT32 Seen; } CPU_SCAN;
typedef struct { CONST OPPO_FDT_VIEW *V; UINT32 Wanted, Definitions; } PHANDLE_SCAN;

STATIC EFI_STATUS EFIAPI PhandleDefinition (CONST CHAR8 *Path, VOID *Context)
{
  PHANDLE_SCAN *Scan = Context;
  CONST VOID *Value;
  UINT32 Bytes, Id[2] = {0, 0};
  CONST CHAR8 *Keys[2] = {"phandle", "linux,phandle"};
  for (UINTN I = 0; I < 2; I++) {
    EFI_STATUS S = OppoFdtProperty (Scan->V, Path, Keys[I], &Value, &Bytes);
    if (S == EFI_NOT_FOUND) continue;
    if (S != EFI_SUCCESS || Bytes != 4) return EFI_UNSUPPORTED;
    Id[I] = Be32 (Value);
    if (Id[I] == 0 || Id[I] == MAX_UINT32) return EFI_UNSUPPORTED;
  }
  if (Id[0] != 0 && Id[1] != 0 && Id[0] != Id[1]) return EFI_UNSUPPORTED;
  if (Id[0] == Scan->Wanted || Id[1] == Scan->Wanted) Scan->Definitions++;
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI Cpu (CONST CHAR8 *Path, VOID *Context)
{
  CPU_SCAN *C = Context;
  CONST VOID *Value;
  UINT32 Bytes;
  UINTN I;
  if (!Starts (Path, "/cpus/")) return EFI_SUCCESS;
  // Only immediate CPU children; their unit-address names are not interpreted.
  if (!Text (C->V, Path, "device_type", "cpu")) {
    return Starts (Path + 6, "cpu@") ? EFI_UNSUPPORTED : EFI_SUCCESS;
  }
  for (I = 6; Path[I] != 0; I++) if (Path[I] == '/') return EFI_UNSUPPORTED;
  if (!Enabled (C->V, Path) || !Text (C->V, Path, "enable-method", "psci") ||
      OppoFdtProperty (C->V, Path, "reg", &Value, &Bytes) != EFI_SUCCESS || Bytes != 4)
    return EFI_UNSUPPORTED;
  UINT32 Affinity = Be32 (Value);
  if ((Affinity & ~0x700U) != 0 || (C->Seen & (1U << (Affinity >> 8))) != 0 ||
      !Text (C->V, Path, "compatible", Affinity < 0x600 ? "arm,cortex-a55" : "arm,cortex-a75"))
    return EFI_UNSUPPORTED;
  C->Seen |= 1U << (Affinity >> 8);
  return EFI_SUCCESS;
}

STATIC BOOLEAN FdtMatches (CONST OPPO_FDT_VIEW *V)
{
  STATIC CONST UINT32 Two[] = {2}, One[] = {1}, Zero[] = {0}, Three[] = {3};
  STATIC CONST UINT32 Reg[] = {0,0x0c000000,0,0x40000, 0,0x0c040000,0,0x200000,
                               0,0x0c53a650,0,0x50};
  STATIC CONST UINT32 Timer[] = {1,13,0xff08, 1,14,0xff08, 1,11,0xff08, 1,10,0xff08};
  STATIC CONST UINT32 Pmu[] = {1,7,8}, Maintenance[] = {1,9,4};
  CONST VOID *Value;
  UINT32 Bytes, Phandle;
  CPU_SCAN Scan = {V, 0};
  if (!Text (V, "/", "compatible", "mediatek,MT6779") ||
      !Numbers (V, "/", "#address-cells", Two, 1) || !Numbers (V, "/", "#size-cells", Two, 1) ||
      !Enabled (V, "/soc") || !Text (V, "/soc", "compatible", "simple-bus") ||
      !Numbers (V, "/soc", "#address-cells", Two, 1) || !Numbers (V, "/soc", "#size-cells", Two, 1) ||
      !Numbers (V, "/soc", "ranges", NULL, 0) ||
      !Enabled (V, "/cpus") || !Numbers (V, "/cpus", "#address-cells", One, 1) ||
      !Numbers (V, "/cpus", "#size-cells", Zero, 1) ||
      OppoFdtVisitNodes (V, Cpu, &Scan) != EFI_SUCCESS || Scan.Seen != 0xffU ||
      !Enabled (V, "/psci") || !Text (V, "/psci", "compatible", "arm,psci-1.0") ||
      !Text (V, "/psci", "method", "smc") ||
      !Enabled (V, GIC_PATH) || !Text (V, GIC_PATH, "compatible", "arm,gic-v3") ||
      !Numbers (V, GIC_PATH, "reg", Reg, ARRAY_SIZE (Reg)) ||
      !Numbers (V, GIC_PATH, "#redistributor-regions", One, 1) ||
      !Numbers (V, GIC_PATH, "#interrupt-cells", Three, 1) ||
      !Numbers (V, GIC_PATH, "interrupt-controller", NULL, 0) ||
      !Numbers (V, GIC_PATH, "interrupts", Maintenance, 3) ||
      OppoFdtProperty (V, GIC_PATH, "phandle", &Value, &Bytes) != EFI_SUCCESS || Bytes != 4)
    return FALSE;
  Phandle = Be32 (Value);
  PHANDLE_SCAN Handles = {V, Phandle, 0};
  if (Phandle == 0 || Phandle == MAX_UINT32 ||
      OppoFdtVisitNodes (V, PhandleDefinition, &Handles) != EFI_SUCCESS || Handles.Definitions != 1 ||
      !Enabled (V, "/timer") || !Text (V, "/timer", "compatible", "arm,armv8-timer") ||
      !Numbers (V, "/timer", "interrupts", Timer, ARRAY_SIZE (Timer)) ||
      !Numbers (V, "/timer", "interrupt-parent", &Phandle, 1) ||
      !Enabled (V, "/pmu") || !Text (V, "/pmu", "compatible", "arm,armv8-pmuv3") ||
      !Numbers (V, "/pmu", "interrupts", Pmu, 3) ||
      !Numbers (V, "/pmu", "interrupt-parent", &Phandle, 1)) return FALSE;
  EFI_STATUS S = OppoFdtProperty (V, GIC_PATH, "redistributor-stride", &Value, &Bytes);
  return (BOOLEAN)(S == EFI_NOT_FOUND || (S == EFI_SUCCESS && Bytes == 8 &&
                   Be32 (Value) == 0 && Be32 ((CONST UINT8 *)Value + 4) == GICR_STRIDE));
}

EFI_STATUS EFIAPI OppoAcpiObserveGic (
  IN CONST VOID *Fdt, IN UINTN Bytes, IN UINT64 RawBootMpidr,
  IN OPPO_GIC_READ32 Read, IN VOID *Context, OUT OPPO_ACPI_GIC_OBSERVATION *Observation
  )
{
  OPPO_FDT_VIEW V;
  OPPO_ACPI_GIC_OBSERVATION Result;
  UINT64 Typer[8], Base[8];
  UINT32 Pidr[8], Seen = 0, Dpidr, Dtype, Dctrl;
  UINT64 Boot = RawBootMpidr & MPIDR_MASK;
  EFI_STATUS Status;
  if (Observation == NULL || Read == NULL || Fdt == NULL ||
      (UINTN)Observation > MAX_UINTN - sizeof (*Observation)) return EFI_INVALID_PARAMETER;
  if (!OppoFdtOpen (Fdt, Bytes, &V)) return EFI_COMPROMISED_DATA;
  if ((UINTN)Observation < (UINTN)Fdt + V.TotalBytes &&
      (UINTN)Fdt < (UINTN)Observation + sizeof (*Observation)) return EFI_INVALID_PARAMETER;
  if ((RawBootMpidr & ~(MPIDR_MASK | BIT31 | BIT24)) != 0 || (Boot & ~0x700ULL) != 0 ||
      !FdtMatches (&V)) return EFI_UNSUPPORTED;
  // All selected FDT properties are checked before the first register read.
#define READ(A,D) do { Status = Read (Context, (A), &(D)); if (Status != EFI_SUCCESS) return Status; } while (0)
  READ (GICD_BASE + PIDR2, Dpidr);
  if (((Dpidr >> 4) & 15U) != 3U) return EFI_UNSUPPORTED;
  READ (GICD_BASE + 4, Dtype); READ (GICD_BASE, Dctrl);
  for (UINTN I = 0; I < 8; I++) {
    UINT64 Address = GICR_BASE + I * GICR_STRIDE;
    UINT32 P, Lo, Hi, AgainLo, AgainHi;
    if (Address > GICR_BASE + GICR_SIZE - GICR_STRIDE) return EFI_UNSUPPORTED;
    READ (Address + PIDR2, P);
    if (((P >> 4) & 15U) != 3U) return EFI_UNSUPPORTED;
    READ (Address + 8, Lo); READ (Address + 12, Hi);
    READ (Address + 8, AgainLo); READ (Address + 12, AgainHi);
    if (Lo != AgainLo || Hi != AgainHi) return EFI_DEVICE_ERROR;
    // VLPIs need a different frame layout; reject before moving to that frame.
    if ((Lo & BIT1) || (Hi & ~0x700U) != 0 || (Seen & (1U << (Hi >> 8))) != 0)
      return EFI_UNSUPPORTED;
    UINT32 CpuIndex = Hi >> 8;
    Seen |= 1U << CpuIndex;
    Typer[CpuIndex] = ((UINT64)Hi << 32) | Lo;
    Base[CpuIndex] = Address; Pidr[CpuIndex] = P;
    if (Lo & BIT4) {
      if (Seen != 0xffU) return EFI_UNSUPPORTED;
      ZeroMem (&Result, sizeof (Result));
      Result.Profile.BootMpidr = Boot;
      Result.DistributorPidr2 = Dpidr; Result.DistributorTyper = Dtype; Result.DistributorControl = Dctrl;
      UINTN Out = 0;
      for (UINTN K = 0; K < 9; K++) {
        UINTN CpuIndexOut = K == 0 ? (UINTN)(Boot >> 8) : K - 1;
        if (K != 0 && CpuIndexOut == (UINTN)(Boot >> 8)) continue;
        Result.Profile.Cpu[Out].Mpidr = CpuIndexOut << 8;
        Result.Profile.Cpu[Out].RedistributorBase = Base[CpuIndexOut];
        Result.RedistributorTyper[Out] = Typer[CpuIndexOut];
        Result.RedistributorPidr2[Out++] = Pidr[CpuIndexOut];
      }
      CopyMem (Observation, &Result, sizeof (Result));
      return EFI_SUCCESS;
    }
  }
#undef READ
  return EFI_UNSUPPORTED; // Eight matching CPUs without Last is not a complete region.
}
