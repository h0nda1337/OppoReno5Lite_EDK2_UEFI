/** @file
  Original ACPI allocation and RSDP publication; references in docs/ACPI.md.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Library/OppoAcpiInstallLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/Acpi.h>
#include <IndustryStandard/Acpi60.h>

STATIC BOOLEAN Overlap (CONST VOID *A, UINTN N, CONST VOID *B, UINTN M)
{
  UINTN X = (UINTN)A, Y = (UINTN)B;
  return (BOOLEAN)(X > MAX_UINTN - N || Y > MAX_UINTN - M || (X < Y + M && Y < X + N));
}

STATIC EFI_STATUS NoExistingAcpi (VOID)
{
  // Bound an internally supplied list before traversing it. This image has only
  // a few configuration tables; a larger list needs an explicit policy review.
  if (gST->NumberOfTableEntries > 256 ||
      (gST->NumberOfTableEntries != 0 && gST->ConfigurationTable == NULL)) return EFI_COMPROMISED_DATA;
  for (UINTN I = 0; I < gST->NumberOfTableEntries; I++) {
    EFI_GUID *G = &gST->ConfigurationTable[I].VendorGuid;
    if (CompareMem (G, &gEfiAcpiTableGuid, sizeof (*G)) == 0 ||
        CompareMem (G, &gEfiAcpi10TableGuid, sizeof (*G)) == 0) return EFI_ALREADY_STARTED;
  }
  return EFI_SUCCESS;
}

STATIC EFI_STATUS ReclaimCoverage (UINT64 Base, UINTN Pages)
{
  UINTN Required = 0, Key, Stride = 0;
  UINT32 Version;
  EFI_STATUS S = gBS->GetMemoryMap (&Required, NULL, &Key, &Stride, &Version);
  if (S != EFI_BUFFER_TOO_SMALL) return EFI_ERROR (S) ? S : EFI_COMPROMISED_DATA;
  for (UINTN Attempt = 0; Attempt < 3; Attempt++) {
    VOID *Map = NULL;
    // Allocation itself can split descriptors. Retry a growing map, bounded to
    // one MiB and a finite number of attempts; no fixed descriptor ABI stride.
    if (Stride < sizeof (EFI_MEMORY_DESCRIPTOR) || Stride > 4096 ||
        Required > 1024U * 1024U - 2U * Stride) return EFI_COMPROMISED_DATA;
    UINTN Capacity = Required + 2 * Stride, Bytes = Capacity;
    S = gBS->AllocatePool (EfiBootServicesData, Capacity, &Map);
    if (EFI_ERROR (S)) return S;
    S = gBS->GetMemoryMap (&Bytes, Map, &Key, &Stride, &Version);
    if (S == EFI_SUCCESS) {
      S = EFI_COMPROMISED_DATA;
      if (Bytes <= Capacity && Stride >= sizeof (EFI_MEMORY_DESCRIPTOR) &&
          Stride <= 4096 && Bytes % Stride == 0 && Version == EFI_MEMORY_DESCRIPTOR_VERSION) {
        for (UINTN At = 0; At < Bytes; At += Stride) {
          EFI_MEMORY_DESCRIPTOR D;
          CopyMem (&D, (CONST UINT8 *)Map + At, sizeof (D));
          if (D.Type == EfiACPIReclaimMemory && D.NumberOfPages <= MAX_UINT64 / EFI_PAGE_SIZE &&
              D.PhysicalStart <= MAX_UINT64 - D.NumberOfPages * EFI_PAGE_SIZE &&
              D.PhysicalStart <= Base && (D.PhysicalStart & EFI_PAGE_MASK) == 0 &&
              Base - D.PhysicalStart <= D.NumberOfPages * EFI_PAGE_SIZE &&
              (UINT64)Pages * EFI_PAGE_SIZE <= D.NumberOfPages * EFI_PAGE_SIZE - (Base - D.PhysicalStart)) {
            S = EFI_SUCCESS; break;
          }
        }
      }
    }
    EFI_STATUS Released = gBS->FreePool (Map);
    if (EFI_ERROR (Released)) return Released;
    if (S != EFI_BUFFER_TOO_SMALL) return S;
    Required = Bytes;
  }
  return EFI_ABORTED;
}

STATIC BOOLEAN SumZero (CONST VOID *Data, UINTN Bytes)
{
  CONST UINT8 *P = Data;
  UINT8 Sum = 0;
  for (UINTN I = 0; I < Bytes; I++) Sum = (UINT8)(Sum + P[I]);
  return (BOOLEAN)(Sum == 0);
}

STATIC EFI_STATUS Readback (CONST OPPO_ACPI_INSTALL_REPORT *R)
{
  UINTN Matches = 0;
  UINT8 *B = (VOID *)(UINTN)R->Base;
  CONST OPPO_ACPI_LAYOUT *L = &R->Layout;
  if (L->TotalBytes != OppoAcpiTableBytes () || L->TotalBytes > R->Pages * EFI_PAGE_SIZE ||
      L->TotalBytes < sizeof (EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER) ||
      L->RsdpOffset > L->TotalBytes - sizeof (EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER))
    return EFI_COMPROMISED_DATA;
  if (gST->NumberOfTableEntries > 256 || gST->ConfigurationTable == NULL) return EFI_COMPROMISED_DATA;
  for (UINTN I = 0; I < gST->NumberOfTableEntries; I++) {
    EFI_CONFIGURATION_TABLE *T = &gST->ConfigurationTable[I];
    if (CompareMem (&T->VendorGuid, &gEfiAcpiTableGuid, sizeof (EFI_GUID)) == 0) {
      if (T->VendorTable != B + L->RsdpOffset) return EFI_COMPROMISED_DATA;
      Matches++;
    }
  }
  if (Matches != 1) return EFI_COMPROMISED_DATA;
  EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp = (VOID *)(B + L->RsdpOffset);
  if (Rsdp->Signature != EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE ||
      Rsdp->Revision != 2 || Rsdp->Length != sizeof (*Rsdp) || Rsdp->RsdtAddress != 0 ||
      Rsdp->XsdtAddress != R->Base + L->XsdtOffset || !SumZero (Rsdp, 20) ||
      !SumZero (Rsdp, sizeof (*Rsdp))) return EFI_COMPROMISED_DATA;
  UINT32 Offsets[] = {L->XsdtOffset, L->FadtOffset, L->DsdtOffset, L->MadtOffset, L->GtdtOffset};
  UINT32 Signatures[] = {SIGNATURE_32 ('X','S','D','T'), SIGNATURE_32 ('F','A','C','P'),
    SIGNATURE_32 ('D','S','D','T'), SIGNATURE_32 ('A','P','I','C'), SIGNATURE_32 ('G','T','D','T')};
  for (UINTN I = 0; I < ARRAY_SIZE (Offsets); I++) {
    if (Offsets[I] > L->TotalBytes - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) return EFI_COMPROMISED_DATA;
    EFI_ACPI_DESCRIPTION_HEADER *H = (VOID *)(B + Offsets[I]);
    if (H->Signature != Signatures[I] || H->Length < sizeof (*H) ||
        H->Length > L->TotalBytes - Offsets[I] || !SumZero (H, H->Length)) return EFI_COMPROMISED_DATA;
  }
  EFI_ACPI_DESCRIPTION_HEADER *Xsdt = (VOID *)(B + L->XsdtOffset);
  UINT64 Pointers[3], Wanted[3] = {R->Base + L->FadtOffset, R->Base + L->MadtOffset, R->Base + L->GtdtOffset};
  if (Xsdt->Length != sizeof (*Xsdt) + sizeof (Pointers)) return EFI_COMPROMISED_DATA;
  CopyMem (Pointers, (UINT8 *)Xsdt + sizeof (*Xsdt), sizeof (Pointers));
  if (CompareMem (Pointers, Wanted, sizeof (Wanted)) != 0) return EFI_COMPROMISED_DATA;
  EFI_ACPI_6_0_FIXED_ACPI_DESCRIPTION_TABLE *Fadt = (VOID *)(B + L->FadtOffset);
  UINT64 Dsdt = R->Base + L->DsdtOffset;
  if (Fadt->Header.Length != sizeof (*Fadt) || Fadt->XDsdt != Dsdt ||
      Fadt->Dsdt != (Dsdt <= MAX_UINT32 ? (UINT32)Dsdt : 0)) return EFI_COMPROMISED_DATA;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI OppoInstallAcpi (
  IN CONST OPPO_ACPI_PROFILE *Profile, OUT OPPO_ACPI_INSTALL_REPORT *Report
  )
{
  EFI_STATUS S;
  if (Profile == NULL || Report == NULL ||
      Overlap (Profile, sizeof (*Profile), Report, sizeof (*Report))) return EFI_INVALID_PARAMETER;
  ZeroMem (Report, sizeof (*Report));
  Report->AllocateStatus = Report->BuildStatus = Report->MemoryStatus =
    Report->PublishStatus = Report->ReadbackStatus = Report->CleanupStatus = EFI_NOT_STARTED;
  if (gST == NULL || gBS == NULL || gBS->AllocatePages == NULL || gBS->FreePages == NULL ||
      gBS->AllocatePool == NULL || gBS->FreePool == NULL || gBS->GetMemoryMap == NULL ||
      gBS->InstallConfigurationTable == NULL) return EFI_UNSUPPORTED;
  S = NoExistingAcpi ();
  if (EFI_ERROR (S)) return S;
  UINTN Bytes = OppoAcpiTableBytes ();
  if (Bytes == 0 || Bytes > MAX_UINTN - EFI_PAGE_MASK) return EFI_BAD_BUFFER_SIZE;
  Report->Pages = EFI_SIZE_TO_PAGES (Bytes);
  S = Report->AllocateStatus = gBS->AllocatePages (AllocateAnyPages, EfiACPIReclaimMemory,
                                                  Report->Pages, &Report->Base);
  if (EFI_ERROR (S)) { Report->Base = 0; Report->Pages = 0; return S; }
  if (Report->Base == 0 || (Report->Base & EFI_PAGE_MASK) != 0 ||
      Report->Base > MAX_UINTN - Report->Pages * EFI_PAGE_SIZE) {
    S = EFI_COMPROMISED_DATA;
  } else {
    // The full allocation is deterministic, including page padding.
    ZeroMem ((VOID *)(UINTN)Report->Base, Report->Pages * EFI_PAGE_SIZE);
    S = Report->BuildStatus = OppoBuildAcpiTables (Profile, (VOID *)(UINTN)Report->Base,
      Report->Pages * EFI_PAGE_SIZE, Report->Base, &Report->Layout);
    if (S == EFI_SUCCESS) S = Report->MemoryStatus = ReclaimCoverage (Report->Base, Report->Pages);
    if (S == EFI_SUCCESS) S = Report->PublishStatus = gBS->InstallConfigurationTable (
      &gEfiAcpiTableGuid, (VOID *)(UINTN)(Report->Base + Report->Layout.RsdpOffset));
    if (S == EFI_SUCCESS) {
      Report->Published = TRUE;
      // Never free memory once a configuration entry may refer to it.
      return Report->ReadbackStatus = Readback (Report);
    }
  }
  Report->CleanupStatus = gBS->FreePages (Report->Base, Report->Pages);
  if (!EFI_ERROR (Report->CleanupStatus)) { Report->Base = 0; Report->Pages = 0; }
  return EFI_ERROR (Report->CleanupStatus) ? Report->CleanupStatus : S;
}
