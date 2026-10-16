/** @file
  MT6779 description derived from the owner's original stock boot DTB.
  See evidence/stock-platform.json for provenance and unresolved runtime data.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Library/OppoPlatformLib.h>

STATIC CONST OPPO_SOC_DESCRIPTION mSoc = {
  8,
  0x0C000000ULL,
  0x0C040000ULL,
  0x10007000ULL,
  0x11002000ULL,
  0x11003000ULL
};

CONST OPPO_SOC_DESCRIPTION *
EFIAPI
OppoGetSocDescription (VOID)
{
  return &mSoc;
}

STATIC
BOOLEAN
RangeValid (
  IN UINT64 Base,
  IN UINT64 Length
  )
{
  return (BOOLEAN)(Length != 0 && Base <= MAX_UINT64 - Length);
}

BOOLEAN
EFIAPI
OppoLoadRegionIsUsable (
  IN BOOLEAN                  RuntimeMapVerified,
  IN UINT64                   Base,
  IN UINT64                   Length,
  IN CONST OPPO_MEMORY_RANGE  *Banks,
  IN UINTN                    BankCount,
  IN CONST OPPO_MEMORY_RANGE  *Reserved,
  IN UINTN                    ReservedCount
  )
{
  UINTN Index;
  UINT64 End;
  BOOLEAN Contained;

  if (!RuntimeMapVerified || !RangeValid (Base, Length) ||
      Banks == NULL || BankCount == 0 ||
      (ReservedCount != 0 && Reserved == NULL)) {
    return FALSE;
  }

  End = Base + Length;
  Contained = FALSE;
  for (Index = 0; Index < BankCount; Index++) {
    if (!RangeValid (Banks[Index].Base, Banks[Index].Length)) {
      return FALSE;
    }
    if (Base >= Banks[Index].Base &&
        End <= Banks[Index].Base + Banks[Index].Length) {
      Contained = TRUE;
    }
  }
  if (!Contained) {
    return FALSE;
  }
  for (Index = 0; Index < ReservedCount; Index++) {
    if (!RangeValid (Reserved[Index].Base, Reserved[Index].Length)) {
      return FALSE;
    }
    if (Base < Reserved[Index].Base + Reserved[Index].Length &&
        Reserved[Index].Base < End) {
      return FALSE;
    }
  }
  return TRUE;
}
