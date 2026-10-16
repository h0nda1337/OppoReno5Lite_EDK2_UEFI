/** @file
  Prepare boot-logo ACPI metadata without accessing a display or physical memory.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_BGRT_LIB_H
#define OPPO_BGRT_LIB_H

#include <Base.h>
#include <IndustryStandard/Acpi66.h>

/**
  Validate a bounded 24-bit BI_RGB BMP and prepare an undisplayed BGRT.

  Bitmap points to readable host/firmware bytes. BitmapAddress is the physical
  address of a separate, persistent EfiBootServicesData copy, allocated by the
  caller. This routine neither allocates that copy nor verifies its contents.
  Screen dimensions and offsets must come from the actual GOP mode.

  No ACPI table is installed and no pixel is drawn. On failure Table is unchanged.
**/
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
  );

/**
  Update status/checksum on an intact table prepared by OppoPrepareBgrt.

  Set TRUE only after the BMP was successfully drawn and is still visible.
  Clear before a Shell, error screen or other UI replaces the image. The caller
  is responsible for updating/reinstalling any copy held by the ACPI protocol.
**/
BOOLEAN
EFIAPI
OppoBgrtSetDisplayed (
  IN OUT EFI_ACPI_6_6_BOOT_GRAPHICS_RESOURCE_TABLE  *Table,
  IN BOOLEAN                                     Displayed
  );

#endif
