/** @file
  Original ACPI byte construction, not an imported platform implementation.
  References and limits: docs/ACPI.md. No runtime hardware access.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Library/OppoAcpiLib.h>
#include <Library/BaseMemoryLib.h>
#include <IndustryStandard/Acpi60.h>

#define GICD_BASE 0x0c000000ULL
#define GICR_BASE 0x0c040000ULL
#define GICR_BYTES 0x200000ULL
#define GICR_FRAME_BYTES 0x20000ULL
#define MPIDR_AFFINITY_MASK 0x000000ff00ffffffULL

// Deliberately a small namespace: eight processors and no hardware methods.
#define AML_DEVICE_BYTES 36U
#define AML_SCOPE_BYTES (1U + 2U + 5U + OPPO_ACPI_CPU_COUNT * AML_DEVICE_BYTES)
#define DSDT_BYTES (sizeof (EFI_ACPI_DESCRIPTION_HEADER) + AML_SCOPE_BYTES)
#define XSDT_BYTES (sizeof (EFI_ACPI_DESCRIPTION_HEADER) + 3U * sizeof (UINT64))
#define MADT_BYTES (sizeof (EFI_ACPI_6_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER) + \
                   OPPO_ACPI_CPU_COUNT * sizeof (EFI_ACPI_6_0_GIC_STRUCTURE) + \
                   sizeof (EFI_ACPI_6_0_GIC_DISTRIBUTOR_STRUCTURE))

STATIC_ASSERT (sizeof (EFI_ACPI_DESCRIPTION_HEADER) == 36, "ACPI header layout");
STATIC_ASSERT (sizeof (EFI_ACPI_6_0_FIXED_ACPI_DESCRIPTION_TABLE) == 276, "FADT layout");
STATIC_ASSERT (sizeof (EFI_ACPI_6_0_GIC_STRUCTURE) == 80, "GICC layout");
STATIC_ASSERT (sizeof (EFI_ACPI_6_0_GENERIC_TIMER_DESCRIPTION_TABLE) == 96, "GTDT layout");
STATIC_ASSERT (2U + 5U + OPPO_ACPI_CPU_COUNT * AML_DEVICE_BYTES < 4096, "AML PkgLength");

STATIC UINT32 Align8 (UINT32 Value) { return (Value + 7U) & ~7U; }

STATIC OPPO_ACPI_LAYOUT TableLayout (VOID)
{
  OPPO_ACPI_LAYOUT L;
  L.RsdpOffset = 0;
  L.XsdtOffset = Align8 (sizeof (EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER));
  L.FadtOffset = Align8 (L.XsdtOffset + XSDT_BYTES);
  L.DsdtOffset = Align8 (L.FadtOffset + sizeof (EFI_ACPI_6_0_FIXED_ACPI_DESCRIPTION_TABLE));
  L.MadtOffset = Align8 (L.DsdtOffset + DSDT_BYTES);
  L.GtdtOffset = Align8 (L.MadtOffset + MADT_BYTES);
  L.TotalBytes = L.GtdtOffset + sizeof (EFI_ACPI_6_0_GENERIC_TIMER_DESCRIPTION_TABLE);
  return L;
}

UINTN EFIAPI OppoAcpiTableBytes (VOID) { return TableLayout ().TotalBytes; }

STATIC BOOLEAN Overlaps (CONST VOID *A, UINTN N, CONST VOID *B, UINTN M)
{
  UINTN X = (UINTN)A, Y = (UINTN)B;
  if (X > MAX_UINTN - N || Y > MAX_UINTN - M) return TRUE;
  return (BOOLEAN)(X < Y + M && Y < X + N);
}

STATIC BOOLEAN ValidProfile (CONST OPPO_ACPI_PROFILE *P)
{
  UINTN I, J;
  UINT32 Seen = 0;
  if ((P->BootMpidr & ~MPIDR_AFFINITY_MASK) != 0 || P->Cpu[0].Mpidr != P->BootMpidr)
    return FALSE;
  for (I = 0; I < OPPO_ACPI_CPU_COUNT; I++) {
    UINT64 M = P->Cpu[I].Mpidr, R = P->Cpu[I].RedistributorBase;
    // The captured DT reg values are 0x000..0x700, NOT the node-name suffixes.
    if ((M & ~0x700ULL) != 0 || (Seen & (1U << (UINT32)(M >> 8))) != 0 ||
        (R & 0xffffULL) != 0 || R < GICR_BASE ||
        R > GICR_BASE + GICR_BYTES - GICR_FRAME_BYTES) return FALSE;
    Seen |= 1U << (UINT32)(M >> 8);
    for (J = 0; J < I; J++) {
      UINT64 Previous = P->Cpu[J].RedistributorBase;
      if (R < Previous + GICR_FRAME_BYTES && Previous < R + GICR_FRAME_BYTES) return FALSE;
    }
  }
  return (BOOLEAN)(Seen == 0xffU);
}

STATIC VOID Checksum (VOID *Data, UINTN Bytes, UINTN Field)
{
  UINT8 *P = Data, Sum = 0;
  UINTN I;
  P[Field] = 0;
  for (I = 0; I < Bytes; I++) Sum = (UINT8)(Sum + P[I]);
  P[Field] = (UINT8)(0U - Sum);
}

STATIC VOID Header (EFI_ACPI_DESCRIPTION_HEADER *H, UINT32 Sig, UINT32 Bytes, UINT8 Rev)
{
  H->Signature = Sig; H->Length = Bytes; H->Revision = Rev;
  CopyMem (H->OemId, "OPNATV", 6);
  H->OemTableId = SIGNATURE_64 ('C','P','H','2','2','0','5',' ');
  H->OemRevision = 1;
  H->CreatorId = SIGNATURE_32 ('O','P','N','V');
  H->CreatorRevision = 1;
}

STATIC VOID CpuNamespace (UINT8 *P)
{
  UINTN I, At = 0;
  UINT32 ScopeLength = 2U + 5U + OPPO_ACPI_CPU_COUNT * AML_DEVICE_BYTES;
  P[At++] = 0x10; // ScopeOp, two-byte PkgLength including its own bytes.
  P[At++] = (UINT8)(0x40U | (ScopeLength & 15U));
  P[At++] = (UINT8)(ScopeLength >> 4);
  CopyMem (P + At, "\\_SB_", 5); At += 5;
  for (I = 0; I < OPPO_ACPI_CPU_COUNT; I++) {
    P[At++] = 0x5b; P[At++] = 0x82; P[At++] = 34; // DeviceOp, one-byte length.
    P[At++] = 'C'; P[At++] = 'P'; P[At++] = '0'; P[At++] = (UINT8)('0' + I);
    P[At++] = 0x08; CopyMem (P + At, "_HID", 4); At += 4;
    P[At++] = 0x0d; CopyMem (P + At, "ACPI0007", 9); At += 9;
    P[At++] = 0x08; CopyMem (P + At, "_UID", 4); At += 4;
    P[At++] = 0x0a; P[At++] = (UINT8)I;
    P[At++] = 0x08; CopyMem (P + At, "_STA", 4); At += 4;
    P[At++] = 0x0a; P[At++] = 0x0f;
  }
}

EFI_STATUS EFIAPI OppoBuildAcpiTables (
  IN CONST OPPO_ACPI_PROFILE *Profile, OUT VOID *Buffer, IN UINTN Bytes,
  IN UINT64 PhysicalBase, OUT OPPO_ACPI_LAYOUT *Layout
  )
{
  OPPO_ACPI_LAYOUT L = TableLayout ();
  UINT8 *B = Buffer;
  UINTN I;
  EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp;
  EFI_ACPI_DESCRIPTION_HEADER *Xsdt, *Dsdt;
  EFI_ACPI_6_0_FIXED_ACPI_DESCRIPTION_TABLE *Fadt;
  EFI_ACPI_6_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *Madt;
  EFI_ACPI_6_0_GENERIC_TIMER_DESCRIPTION_TABLE *Gtdt;
  EFI_ACPI_6_0_GIC_DISTRIBUTOR_STRUCTURE *Gicd;
  if (Profile == NULL || Buffer == NULL || Layout == NULL || ((UINTN)B & 7U) != 0 ||
      PhysicalBase == 0 || (PhysicalBase & 15U) != 0 ||
      PhysicalBase > MAX_UINT64 - L.TotalBytes ||
      Overlaps (Buffer, L.TotalBytes, Profile, sizeof (*Profile)) ||
      Overlaps (Buffer, L.TotalBytes, Layout, sizeof (*Layout)) ||
      Overlaps (Profile, sizeof (*Profile), Layout, sizeof (*Layout))) return EFI_INVALID_PARAMETER;
  if (Bytes < L.TotalBytes) return EFI_BUFFER_TOO_SMALL;
  if (!ValidProfile (Profile)) return EFI_UNSUPPORTED;

  // All reject paths precede the first write. Every omitted/reserved byte is zero.
  ZeroMem (Buffer, L.TotalBytes);
  Rsdp = (VOID *)(B + L.RsdpOffset); Xsdt = (VOID *)(B + L.XsdtOffset);
  Fadt = (VOID *)(B + L.FadtOffset); Dsdt = (VOID *)(B + L.DsdtOffset);
  Madt = (VOID *)(B + L.MadtOffset); Gtdt = (VOID *)(B + L.GtdtOffset);
  Rsdp->Signature = EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE;
  CopyMem (Rsdp->OemId, "OPNATV", 6);
  Rsdp->Revision = 2; Rsdp->Length = sizeof (*Rsdp);
  Rsdp->XsdtAddress = PhysicalBase + L.XsdtOffset;
  Checksum (Rsdp, 20, 8); Checksum (Rsdp, sizeof (*Rsdp), 32);

  Header (Xsdt, SIGNATURE_32 ('X','S','D','T'), XSDT_BYTES, 1);
  UINT64 Entries[3] = {PhysicalBase + L.FadtOffset, PhysicalBase + L.MadtOffset,
                       PhysicalBase + L.GtdtOffset};
  CopyMem ((UINT8 *)Xsdt + sizeof (*Xsdt), Entries, sizeof (Entries));
  Checksum (Xsdt, XSDT_BYTES, 9);

  Header (&Fadt->Header, SIGNATURE_32 ('F','A','C','P'), sizeof (*Fadt),
          EFI_ACPI_6_0_FIXED_ACPI_DESCRIPTION_TABLE_REVISION);
  Fadt->PreferredPmProfile = EFI_ACPI_6_0_PM_PROFILE_TABLET;
  Fadt->Flags = EFI_ACPI_6_0_HW_REDUCED_ACPI;
  Fadt->ArmBootArch = EFI_ACPI_6_0_ARM_PSCI_COMPLIANT; // SMC, not HVC.
  Fadt->XDsdt = PhysicalBase + L.DsdtOffset;
  if (Fadt->XDsdt <= MAX_UINT32) Fadt->Dsdt = (UINT32)Fadt->XDsdt;
  Checksum (Fadt, sizeof (*Fadt), 9);

  Header (Dsdt, SIGNATURE_32 ('D','S','D','T'), DSDT_BYTES, 2);
  CpuNamespace ((UINT8 *)Dsdt + sizeof (*Dsdt));
  Checksum (Dsdt, DSDT_BYTES, 9);

  Header (&Madt->Header, SIGNATURE_32 ('A','P','I','C'), MADT_BYTES,
          EFI_ACPI_6_0_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION);
  for (I = 0; I < OPPO_ACPI_CPU_COUNT; I++) {
    EFI_ACPI_6_0_GIC_STRUCTURE *C = (VOID *)((UINT8 *)Madt + sizeof (*Madt) + I * sizeof (*C));
    C->Type = 0x0b; C->Length = sizeof (*C);
    C->CPUInterfaceNumber = 0; // No GICv2 compatibility interface is described.
    C->AcpiProcessorUid = (UINT32)I;
    C->Flags = EFI_ACPI_6_0_GIC_ENABLED;
    C->PerformanceInterruptGsiv = 23; // Captured /pmu: GIC_PPI 7, level low.
    C->VGICMaintenanceInterrupt = 25; // DT GIC_PPI 9, level high.
    C->GICRBaseAddress = Profile->Cpu[I].RedistributorBase;
    C->MPIDR = Profile->Cpu[I].Mpidr;
    // No invented efficiency class, parking mailbox or GICv2 MMIO.
  }
  Gicd = (VOID *)((UINT8 *)Madt + sizeof (*Madt) +
                 OPPO_ACPI_CPU_COUNT * sizeof (EFI_ACPI_6_0_GIC_STRUCTURE));
  Gicd->Type = 0x0c; Gicd->Length = sizeof (*Gicd);
  Gicd->PhysicalBaseAddress = GICD_BASE; Gicd->GicVersion = EFI_ACPI_6_0_GIC_V3;
  Checksum (Madt, MADT_BYTES, 9);

  Header (&Gtdt->Header, SIGNATURE_32 ('G','T','D','T'), sizeof (*Gtdt),
          EFI_ACPI_6_0_GENERIC_TIMER_DESCRIPTION_TABLE_REVISION);
  Gtdt->CntControlBasePhysicalAddress = MAX_UINT64;
  Gtdt->CntReadBasePhysicalAddress = MAX_UINT64; // No memory-mapped counter supplied.
  Gtdt->SecurePL1TimerGSIV = 29; Gtdt->SecurePL1TimerFlags = BIT1;
  Gtdt->NonSecurePL1TimerGSIV = 30; Gtdt->NonSecurePL1TimerFlags = BIT1;
  Gtdt->VirtualTimerGSIV = 27; Gtdt->VirtualTimerFlags = BIT1;
  Gtdt->NonSecurePL2TimerGSIV = 26; Gtdt->NonSecurePL2TimerFlags = BIT1;
  // DT low-nibble flag 8 = level low. No always-on guarantee is invented.
  Checksum (Gtdt, sizeof (*Gtdt), 9);
  *Layout = L;
  return EFI_SUCCESS;
}
