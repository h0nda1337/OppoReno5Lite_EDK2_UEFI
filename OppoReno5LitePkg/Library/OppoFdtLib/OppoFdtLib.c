/** @file
  Original FDT token reader. Format references: docs/NATIVE_ENTRY.md.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Library/OppoFdtLib.h>

#define FDT_LIMIT (2U * 1024 * 1024)
#define DEPTH_LIMIT 32
#define PATH_LIMIT 1024

STATIC UINT32 Be32 (IN CONST UINT8 *P)
{
  return ((UINT32)P[0] << 24) | ((UINT32)P[1] << 16) | ((UINT32)P[2] << 8) | P[3];
}

STATIC UINT64 Be64 (IN CONST UINT8 *P)
{
  return ((UINT64)Be32 (P) << 32) | Be32 (P + 4);
}

STATIC BOOLEAN Inside (UINT32 Offset, UINT32 Length, UINT32 Total)
{
  return (BOOLEAN)(Offset <= Total && Length <= Total - Offset);
}

STATIC BOOLEAN Overlap (UINT32 A, UINT32 N, UINT32 B, UINT32 M)
{
  // Called only after both ranges have been checked against the 2 MiB total.
  return (BOOLEAN)(N != 0 && M != 0 && A < B + M && B < A + N);
}

STATIC BOOLEAN TextEquals (IN CONST CHAR8 *A, IN CONST CHAR8 *B)
{
  UINTN Index;
  for (Index = 0; Index < PATH_LIMIT; Index++) {
    if (A[Index] != B[Index]) return FALSE;
    if (A[Index] == 0) return TRUE;
  }
  return FALSE;
}

STATIC BOOLEAN TextLength (IN CONST UINT8 *Data, UINT32 Start, UINT32 End, OUT UINT32 *Length)
{
  UINT32 Index;
  for (Index = Start; Index < End; Index++) {
    if (Data[Index] == 0) { *Length = Index - Start; return TRUE; }
    if (Data[Index] < 0x21 || Data[Index] > 0x7e || Data[Index] == '/') return FALSE;
    if (Index - Start >= 127) return FALSE;
  }
  return FALSE;
}

STATIC BOOLEAN Header (IN CONST VOID *Buffer, UINTN Bytes, OUT OPPO_FDT_VIEW *View)
{
  OPPO_FDT_VIEW V;
  CONST UINT8 *Data = Buffer;
  UINT32 Position, Index;
  BOOLEAN Terminated = FALSE;
  if (Data == NULL || View == NULL || Bytes < 40 || Be32 (Data) != 0xd00dfeed) return FALSE;
  V.Data = Data;
  V.TotalBytes = Be32 (Data + 4);
  V.StructureOffset = Be32 (Data + 8); V.StringsOffset = Be32 (Data + 12);
  V.ReserveOffset = Be32 (Data + 16); V.StringsBytes = Be32 (Data + 32);
  V.StructureBytes = Be32 (Data + 36); V.ReserveCount = 0;
  if (V.TotalBytes < 40 || V.TotalBytes > FDT_LIMIT || V.TotalBytes > Bytes ||
      (UINTN)Data > MAX_UINTN - V.TotalBytes || Be32 (Data + 20) < 17 ||
      Be32 (Data + 24) < 16 || Be32 (Data + 24) > 17 ||
      V.StructureOffset < 40 || V.StringsOffset < 40 || V.ReserveOffset < 40 ||
      V.StructureOffset % 4 || V.StructureBytes % 4 || V.ReserveOffset % 8 ||
      V.StructureBytes < 16 || !Inside (V.StructureOffset, V.StructureBytes, V.TotalBytes) ||
      !Inside (V.StringsOffset, V.StringsBytes, V.TotalBytes) ||
      Overlap (V.StructureOffset, V.StructureBytes, V.StringsOffset, V.StringsBytes)) return FALSE;
  Position = V.ReserveOffset;
  while (V.ReserveCount <= 128) {
    UINT64 Base, Size;
    if (!Inside (Position, 16, V.TotalBytes) ||
        Overlap (Position, 16, V.StructureOffset, V.StructureBytes) ||
        Overlap (Position, 16, V.StringsOffset, V.StringsBytes)) return FALSE;
    Base = Be64 (Data + Position); Size = Be64 (Data + Position + 8);
    if (Base == 0 && Size == 0) { Terminated = TRUE; break; }
    if (V.ReserveCount == 128 || Size == 0 || Base > MAX_UINT64 - Size) return FALSE;
    for (Index = 0; Index < V.ReserveCount; Index++) {
      CONST UINT8 *Earlier = Data + V.ReserveOffset + Index * 16;
      UINT64 PreviousBase = Be64 (Earlier), PreviousSize = Be64 (Earlier + 8);
      if (Base < PreviousBase + PreviousSize && PreviousBase < Base + Size) return FALSE;
    }
    V.ReserveCount++;
    Position += 16;
  }
  if (!Terminated) return FALSE;
  *View = V;
  return TRUE;
}

STATIC BOOLEAN Scan (
  IN CONST OPPO_FDT_VIEW *V, IN CONST CHAR8 *WantedPath OPTIONAL,
  IN CONST CHAR8 *WantedName OPTIONAL, OUT CONST VOID **Value OPTIONAL,
  OUT UINT32 *Bytes OPTIONAL, IN OPPO_FDT_NODE_VISITOR Visitor OPTIONAL,
  IN OUT VOID *Context OPTIONAL, OUT EFI_STATUS *VisitorStatus OPTIONAL
  )
{
  UINT32 Position = V->StructureOffset, End = Position + V->StructureBytes;
  UINT32 Depth = 0, Nodes = 0, PathLength = 0, Saved[DEPTH_LIMIT];
  UINT32 Matches = 0;
  BOOLEAN Children[DEPTH_LIMIT], Root = FALSE;
  CHAR8 Path[PATH_LIMIT];
  Path[0] = 0;
  while (Position < End) {
    UINT32 Token;
    if (!Inside (Position, 4, End)) return FALSE;
    Token = Be32 (V->Data + Position); Position += 4;
    if (Token == 1) {
      UINT32 Length, Index;
      if (Depth == DEPTH_LIMIT || ++Nodes > 4096 ||
          !TextLength (V->Data, Position, End, &Length)) return FALSE;
      if (Depth == 0) {
        if (Root || Length != 0) return FALSE;
        Root = TRUE;
      } else if (Length == 0) return FALSE;
      if (Depth != 0) Children[Depth - 1] = TRUE;
      Saved[Depth] = PathLength; Children[Depth] = FALSE;
      if (Depth == 0 || Depth > 1) {
        if (PathLength + 1 >= PATH_LIMIT) return FALSE;
        Path[PathLength++] = '/';
      }
      if (Length >= PATH_LIMIT - PathLength) return FALSE;
      for (Index = 0; Index < Length; Index++) Path[PathLength++] = (CHAR8)V->Data[Position + Index];
      Path[PathLength] = 0;
      if (WantedPath != NULL && TextEquals (Path, WantedPath)) {
        if (++Matches > 1) return FALSE;
        if (WantedName == NULL) *Value = V->Data + Position;
      }
      if (Visitor != NULL) {
        *VisitorStatus = Visitor (Path, Context);
        if (*VisitorStatus != EFI_SUCCESS) return FALSE;
      }
      Position = (Position + Length + 4) & ~3U;
      if (Position > End) return FALSE;
      Depth++;
    } else if (Token == 2) {
      if (Depth == 0) return FALSE;
      PathLength = Saved[--Depth]; Path[PathLength] = 0;
    } else if (Token == 3) {
      UINT32 Length, NameOffset, NameLength;
      if (Depth == 0 || Children[Depth - 1] || !Inside (Position, 8, End)) return FALSE;
      Length = Be32 (V->Data + Position); NameOffset = Be32 (V->Data + Position + 4); Position += 8;
      if (!Inside (Position, Length, End) || NameOffset >= V->StringsBytes ||
          !TextLength (V->Data, V->StringsOffset + NameOffset,
                       V->StringsOffset + V->StringsBytes, &NameLength) || NameLength == 0) return FALSE;
      if (WantedPath != NULL && WantedName == NULL && TextEquals (Path, WantedPath)) {
        (*Bytes)++;
      } else if (WantedPath != NULL && WantedName != NULL && TextEquals (Path, WantedPath) &&
          TextEquals ((CONST CHAR8 *)V->Data + V->StringsOffset + NameOffset, WantedName)) {
        if (*Value != NULL) return FALSE;
        *Value = V->Data + Position; *Bytes = Length;
      }
      Position = (Position + Length + 3) & ~3U;
      if (Position > End) return FALSE;
    } else if (Token == 9) {
      return (BOOLEAN)(Root && Depth == 0 && Position == End);
    } else if (Token != 4) return FALSE;
  }
  return FALSE;
}

BOOLEAN EFIAPI OppoFdtOpen (IN CONST VOID *Data, IN UINTN Bytes, OUT OPPO_FDT_VIEW *View)
{
  OPPO_FDT_VIEW Candidate;
  if (View == NULL || !Header (Data, Bytes, &Candidate) ||
      !Scan (&Candidate, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) return FALSE;
  *View = Candidate;
  return TRUE;
}

EFI_STATUS EFIAPI OppoFdtProperty (
  IN CONST OPPO_FDT_VIEW *View, IN CONST CHAR8 *Path, IN CONST CHAR8 *Name,
  OUT CONST VOID **Value, OUT UINT32 *Bytes
  )
{
  OPPO_FDT_VIEW Checked;
  if (Value == NULL || Bytes == NULL) return EFI_INVALID_PARAMETER;
  *Value = NULL; *Bytes = 0;
  if (View == NULL || Path == NULL || Name == NULL || Path[0] != '/' || Name[0] == 0)
    return EFI_INVALID_PARAMETER;
  if (!Header (View->Data, View->TotalBytes, &Checked) ||
      !Scan (&Checked, Path, Name, Value, Bytes, NULL, NULL, NULL)) {
    *Value = NULL; *Bytes = 0;
    return EFI_COMPROMISED_DATA;
  }
  return *Value == NULL ? EFI_NOT_FOUND : EFI_SUCCESS;
}

BOOLEAN EFIAPI OppoFdtReservation (IN CONST OPPO_FDT_VIEW *View, IN UINT32 Index, OUT OPPO_MEMORY_RANGE *Range)
{
  OPPO_FDT_VIEW Checked;
  CONST UINT8 *Entry;
  if (View == NULL || Range == NULL || !Header (View->Data, View->TotalBytes, &Checked) ||
      Index >= Checked.ReserveCount) return FALSE;
  Entry = Checked.Data + Checked.ReserveOffset + Index * 16;
  Range->Base = Be64 (Entry); Range->Length = Be64 (Entry + 8);
  return TRUE;
}

EFI_STATUS EFIAPI OppoFdtVisitNodes (
  IN CONST OPPO_FDT_VIEW *View, IN OPPO_FDT_NODE_VISITOR Visitor,
  IN OUT VOID *Context
  )
{
  OPPO_FDT_VIEW Checked;
  EFI_STATUS Status = EFI_SUCCESS;
  if (View == NULL || Visitor == NULL) return EFI_INVALID_PARAMETER;
  if (!OppoFdtOpen (View->Data, View->TotalBytes, &Checked)) return EFI_COMPROMISED_DATA;
  if (!Scan (&Checked, NULL, NULL, NULL, NULL, Visitor, Context, &Status))
    return Status == EFI_SUCCESS ? EFI_COMPROMISED_DATA : Status;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI OppoFdtPropertyCount (
  IN CONST OPPO_FDT_VIEW *View, IN CONST CHAR8 *Path, OUT UINT32 *Count
  )
{
  OPPO_FDT_VIEW Checked;
  CONST VOID *Node = NULL;
  if (View == NULL || Path == NULL || Path[0] != '/' || Count == NULL) return EFI_INVALID_PARAMETER;
  *Count = 0;
  if (!Header (View->Data, View->TotalBytes, &Checked) ||
      !Scan (&Checked, Path, NULL, &Node, Count, NULL, NULL, NULL)) {
    *Count = 0;
    return EFI_COMPROMISED_DATA;
  }
  return Node == NULL ? EFI_NOT_FOUND : EFI_SUCCESS;
}
