/** @file
  Bounded BMP validation and ACPI BGRT construction; no device operations.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Library/OppoBgrtLib.h>

STATIC_ASSERT (sizeof (EFI_ACPI_6_6_BOOT_GRAPHICS_RESOURCE_TABLE) == 56, "BGRT size");

STATIC
UINT32
ReadLe32 (IN CONST UINT8 *Data)
{
  return (UINT32)Data[0] | ((UINT32)Data[1] << 8) |
         ((UINT32)Data[2] << 16) | ((UINT32)Data[3] << 24);
}

STATIC
UINT16
ReadLe16 (IN CONST UINT8 *Data)
{
  return (UINT16)((UINT16)Data[0] | ((UINT16)Data[1] << 8));
}

STATIC
UINT8
ByteSum (IN CONST VOID *Buffer, IN UINTN Size)
{
  CONST UINT8  *Bytes = Buffer;
  UINT8        Sum = 0;
  UINTN        Index;

  for (Index = 0; Index < Size; ++Index) {
    Sum = (UINT8)(Sum + Bytes[Index]);
  }

  return Sum;
}

STATIC
VOID
UpdateChecksum (IN OUT EFI_ACPI_6_6_BOOT_GRAPHICS_RESOURCE_TABLE *Table)
{
  Table->Header.Checksum = 0;
  Table->Header.Checksum = (UINT8)(0 - ByteSum (Table, sizeof (*Table)));
}

BOOLEAN
EFIAPI
OppoPrepareBgrt (
  OUT EFI_ACPI_6_6_BOOT_GRAPHICS_RESOURCE_TABLE  *Table,
  IN CONST VOID                               *Bitmap,
  IN UINTN                                    BitmapSize,
  IN UINT64                                   BitmapAddress,
  IN UINT32                                   ScreenWidth,
  IN UINT32                                   ScreenHeight,
  IN UINT32                                   OffsetX,
  IN UINT32                                   OffsetY
  )
{
  CONST UINT8                                 *Bytes = Bitmap;
  EFI_ACPI_6_6_BOOT_GRAPHICS_RESOURCE_TABLE     Result = {0};
  UINT32                                      Width;
  UINT32                                      Height;
  UINT64                                      PixelsSize;
  UINTN                                       Index;
  CONST UINT8                                 OemId[6] = {'O','P','P','O',' ',' '};

  if ((Table == NULL) || (Bitmap == NULL) || (BitmapSize < 54) ||
      (BitmapSize > 32 * 1024 * 1024) || (BitmapAddress == 0) ||
      (BitmapAddress > MAX_UINT64 - BitmapSize)) {
    return FALSE;
  }

  // The extractor emits this specific, palette-free bottom-up BMP format.
  if ((Bytes[0] != 'B') || (Bytes[1] != 'M') ||
      (ReadLe32 (Bytes + 2) != BitmapSize) || (ReadLe32 (Bytes + 6) != 0) ||
      (ReadLe32 (Bytes + 10) != 54) || (ReadLe32 (Bytes + 14) != 40) ||
      (ReadLe16 (Bytes + 26) != 1) || (ReadLe16 (Bytes + 28) != 24) ||
      (ReadLe32 (Bytes + 30) != 0) || (ReadLe32 (Bytes + 46) != 0) ||
      (ReadLe32 (Bytes + 50) != 0)) {
    return FALSE;
  }

  Width = ReadLe32 (Bytes + 18);
  Height = ReadLe32 (Bytes + 22);
  if ((Width == 0) || (Height == 0) || (Width > 4096) || (Height > 4096) ||
      (ScreenWidth == 0) || (ScreenHeight == 0) ||
      (OffsetX > ScreenWidth) || (Width > ScreenWidth - OffsetX) ||
      (OffsetY > ScreenHeight) || (Height > ScreenHeight - OffsetY)) {
    return FALSE;
  }

  PixelsSize = (((UINT64)Width * 3 + 3) & ~((UINT64)3)) * Height;
  if ((PixelsSize + 54 != BitmapSize) ||
      (ReadLe32 (Bytes + 34) != PixelsSize)) {
    return FALSE;
  }

  Result.Header.Signature = EFI_ACPI_6_6_BOOT_GRAPHICS_RESOURCE_TABLE_SIGNATURE;
  Result.Header.Length = sizeof (Result);
  Result.Header.Revision = EFI_ACPI_6_6_BOOT_GRAPHICS_RESOURCE_TABLE_REVISION;
  for (Index = 0; Index < sizeof (OemId); ++Index) {
    Result.Header.OemId[Index] = OemId[Index];
  }

  // Project identifiers, not a claim that this table was issued by OPPO.
  Result.Header.OemTableId = SIGNATURE_64 ('C','P','H','2','2','0','5',' ');
  Result.Header.OemRevision = 1;
  Result.Header.CreatorId = SIGNATURE_32 ('O','R','5','L');
  Result.Header.CreatorRevision = 1;
  Result.Version = EFI_ACPI_6_6_BGRT_VERSION;
  Result.Status = EFI_ACPI_6_6_BGRT_STATUS_NOT_DISPLAYED;
  Result.ImageType = EFI_ACPI_6_6_BGRT_IMAGE_TYPE_BMP;
  Result.ImageAddress = BitmapAddress;
  Result.ImageOffsetX = OffsetX;
  Result.ImageOffsetY = OffsetY;
  UpdateChecksum (&Result);
  *Table = Result;
  return TRUE;
}

BOOLEAN
EFIAPI
OppoBgrtSetDisplayed (
  IN OUT EFI_ACPI_6_6_BOOT_GRAPHICS_RESOURCE_TABLE  *Table,
  IN BOOLEAN                                     Displayed
  )
{
  if ((Table == NULL) ||
      (Table->Header.Signature != EFI_ACPI_6_6_BOOT_GRAPHICS_RESOURCE_TABLE_SIGNATURE) ||
      (Table->Header.Length != sizeof (*Table)) ||
      (Table->Header.Revision != EFI_ACPI_6_6_BOOT_GRAPHICS_RESOURCE_TABLE_REVISION) ||
      (Table->Version != EFI_ACPI_6_6_BGRT_VERSION) ||
      (Table->Status > EFI_ACPI_6_6_BGRT_STATUS_DISPLAYED) ||
      (Table->ImageType != EFI_ACPI_6_6_BGRT_IMAGE_TYPE_BMP) ||
      (Table->ImageAddress == 0) || (ByteSum (Table, sizeof (*Table)) != 0)) {
    return FALSE;
  }

  Table->Status = Displayed ? EFI_ACPI_6_6_BGRT_STATUS_DISPLAYED :
                             EFI_ACPI_6_6_BGRT_STATUS_NOT_DISPLAYED;
  UpdateChecksum (Table);
  return TRUE;
}
