/** @file
  Original bounded framebuffer operations for a future OPPO GOP driver.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_FRAMEBUFFER_LIB_H
#define OPPO_FRAMEBUFFER_LIB_H

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>

typedef struct {
  UINT32  Width;
  UINT32  Height;
  UINT32  StrideBytes;
  UINT8   RedShift;
  UINT8   BlueShift;
  BOOLEAN OpaqueAlpha;
} OPPO_FB_LAYOUT;

typedef struct {
  volatile UINT32  *Pixels;
  UINTN             MappedBytes;
  OPPO_FB_LAYOUT    Layout;
} OPPO_FB_CONTEXT;

/**
  Check software bounds and initialize a context without writing its buffer.

  The caller must establish that Memory is already mapped and reserved for the
  active display, and perform any required cache maintenance. This library does
  not map physical memory, program the panel, or establish a hardware handoff.
  Supported layouts are RGBX/BGRX, with green in bits 8..15. When OpaqueAlpha is
  TRUE, color writes set bits 24..31 to avoid transparent colors with active alpha.
**/
BOOLEAN
EFIAPI
OppoFbInitialize (
  OUT OPPO_FB_CONTEXT       *Context,
  IN VOID                  *Memory,
  IN UINTN                  MappedBytes,
  IN CONST OPPO_FB_LAYOUT   *Layout
  );

/**
  Implement the four GOP-style BLT operations with explicit buffer bounds.

  BltBytes is the caller's allocation size. Delta zero means Width pixels per
  buffer row. Buffer operations require a separate allocation; use VideoToVideo
  for overlapping framebuffer rectangles. No allocation or MMIO is performed.
  This is a software primitive, not an installed Graphics Output Protocol.
**/
EFI_STATUS
EFIAPI
OppoFbBlt (
  IN OUT OPPO_FB_CONTEXT                *Context,
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *BltBuffer OPTIONAL,
  IN UINTN                              BltBytes,
  IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION   Operation,
  IN UINTN                              SourceX,
  IN UINTN                              SourceY,
  IN UINTN                              DestinationX,
  IN UINTN                              DestinationY,
  IN UINTN                              Width,
  IN UINTN                              Height,
  IN UINTN                              Delta
  );

#endif
