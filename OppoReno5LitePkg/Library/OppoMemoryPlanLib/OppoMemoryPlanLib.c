/** @file
  Original early memory policy; static DT reservations stay protected.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Library/OppoMemoryPlanLib.h>

STATIC BOOLEAN AlignedRange (OPPO_MEMORY_RANGE R)
{
  return (BOOLEAN)(R.Length != 0 && R.Base <= MAX_UINT64 - R.Length &&
         (R.Base & 4095) == 0 && (R.Length & 4095) == 0);
}

EFI_STATUS EFIAPI OppoPlanEarlyMemory (
  IN CONST OPPO_BOOT_HANDOFF *Handoff, IN OPPO_MEMORY_RANGE Image,
  IN OPPO_MEMORY_RANGE Pool, OUT OPPO_EARLY_MEMORY_PLAN *Plan
  )
{
  OPPO_EARLY_MEMORY_PLAN Candidate;
  UINTN Index, Earlier;
  if (Handoff == NULL || Plan == NULL || !AlignedRange (Image) || !AlignedRange (Pool) ||
      Pool.Length < 16 * 1024 * 1024 || Pool.Length > 256 * 1024 * 1024 ||
      Image.Length > 64 * 1024 * 1024) return EFI_INVALID_PARAMETER;
  if (!Handoff->FdtPhysicalAddressRecorded || Handoff->UnresolvedReservationCount != 0 ||
      Handoff->RamCount == 0 || Handoff->RamCount > OPPO_MAX_RAM_BANKS ||
      Handoff->ReservedCount > OPPO_MAX_BOOT_RESERVATIONS) return EFI_UNSUPPORTED;
  volatile UINT8 *Clear = (volatile UINT8 *)&Candidate;
  for (Index = 0; Index < sizeof (Candidate); Index++) Clear[Index] = 0;
  Candidate.Image = Image; Candidate.Pool = Pool;
  Candidate.DynamicReservationsDeferredToOs = Handoff->DynamicReservationCount;
  Candidate.EmptyNodesIgnored = Handoff->EmptyReservationNodeCount;
  for (Index = 0; Index < Handoff->ReservedCount; Index++) {
    OPPO_MEMORY_RANGE R = Handoff->Reserved[Index];
    UINT32 Flags = Handoff->ReservedFlags[Index];
    UINT64 End;
    if (R.Length == 0 || R.Base > MAX_UINT64 - R.Length ||
        R.Base + R.Length > MAX_UINT64 - 4095 || (Flags & ~7U) != 0)
      return EFI_COMPROMISED_DATA;
    End = (R.Base + R.Length + 4095) & ~4095ULL;
    R.Base &= ~4095ULL; R.Length = End - R.Base;
    for (Earlier = 0; Earlier < Index; Earlier++) {
      OPPO_MEMORY_RANGE P = Candidate.Protected[Earlier];
      if (R.Base < P.Base + P.Length && P.Base < R.Base + R.Length)
        return EFI_COMPROMISED_DATA;
    }
    Candidate.Protected[Index] = R;
    Candidate.ProtectedType[Index] = Flags & (OPPO_RESERVATION_NO_MAP | OPPO_RESERVATION_MEMRESERVE) ?
                                      EfiReservedMemoryType : EfiBootServicesData;
    Candidate.ProtectedCount++;
  }
  if (!OppoLoadRegionIsUsable (TRUE, Image.Base, Image.Length, Handoff->Ram, Handoff->RamCount,
                              Candidate.Protected, Candidate.ProtectedCount)) return EFI_ACCESS_DENIED;
  Candidate.Protected[Candidate.ProtectedCount] = Image;
  Candidate.ProtectedType[Candidate.ProtectedCount++] = EfiBootServicesCode;
  if (!OppoLoadRegionIsUsable (TRUE, Pool.Base, Pool.Length, Handoff->Ram, Handoff->RamCount,
                              Candidate.Protected, Candidate.ProtectedCount)) return EFI_ACCESS_DENIED;
  volatile UINT8 *Destination = (volatile UINT8 *)Plan;
  CONST UINT8 *Source = (CONST UINT8 *)&Candidate;
  for (Index = 0; Index < sizeof (Candidate); Index++) Destination[Index] = Source[Index];
  return EFI_SUCCESS;
}
