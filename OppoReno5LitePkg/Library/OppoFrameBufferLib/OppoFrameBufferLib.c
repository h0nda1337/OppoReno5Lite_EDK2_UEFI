/** @file
  Original software BLT implementation; no borrowed platform driver code.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Library/OppoFrameBufferLib.h>

STATIC
BOOLEAN
ContextValid (IN CONST OPPO_FB_CONTEXT *Context)
{
  CONST OPPO_FB_LAYOUT *Layout;
  if (Context == NULL || Context->Pixels == NULL ||
      ((UINTN)Context->Pixels & 3) != 0 || Context->MappedBytes > 256 * 1024 * 1024 ||
      (UINTN)Context->Pixels > MAX_UINTN - Context->MappedBytes) {
    return FALSE;
  }
  Layout = &Context->Layout;
  return (BOOLEAN)(Layout->Width != 0 && Layout->Width <= 4096 &&
         Layout->Height != 0 && Layout->Height <= 8192 &&
         Layout->StrideBytes <= 65536 && Layout->StrideBytes % 4 == 0 &&
         Layout->StrideBytes >= Layout->Width * 4 &&
         (UINT64)Layout->StrideBytes * Layout->Height <= Context->MappedBytes &&
         ((Layout->RedShift == 0 && Layout->BlueShift == 16) ||
          (Layout->RedShift == 16 && Layout->BlueShift == 0)));
}

BOOLEAN
EFIAPI
OppoFbInitialize (
  OUT OPPO_FB_CONTEXT      *Context,
  IN VOID                 *Memory,
  IN UINTN                 MappedBytes,
  IN CONST OPPO_FB_LAYOUT  *Layout
  )
{
  OPPO_FB_CONTEXT Candidate;
  if (Context == NULL || Layout == NULL) {
    return FALSE;
  }
  Candidate.Pixels = Memory;
  Candidate.MappedBytes = MappedBytes;
  Candidate.Layout = *Layout;
  if (!ContextValid (&Candidate)) {
    return FALSE;
  }
  *Context = Candidate;
  return TRUE;
}

STATIC
BOOLEAN
RectangleFits (
  IN CONST OPPO_FB_CONTEXT *Context,
  IN UINTN X, IN UINTN Y, IN UINTN Width, IN UINTN Height
  )
{
  return (BOOLEAN)(X <= Context->Layout.Width && Y <= Context->Layout.Height &&
         Width <= Context->Layout.Width - X && Height <= Context->Layout.Height - Y);
}

STATIC
BOOLEAN
BufferFits (
  IN CONST OPPO_FB_CONTEXT *Context,
  IN CONST VOID *Buffer, IN UINTN Bytes, IN UINTN X, IN UINTN Y,
  IN UINTN Width, IN UINTN Height, IN UINTN Delta
  )
{
  UINTN LastRow, RowEnd, Required;
  if (Buffer == NULL || Delta % 4 != 0 || X > MAX_UINTN / 4 ||
      Width > MAX_UINTN / 4 - X || Height - 1 > MAX_UINTN - Y) {
    return FALSE;
  }
  RowEnd = (X + Width) * 4;
  LastRow = Y + Height - 1;
  if (Delta < RowEnd || (LastRow != 0 && Delta > (MAX_UINTN - RowEnd) / LastRow)) {
    return FALSE;
  }
  Required = LastRow * Delta + RowEnd;
  if (Required > Bytes || (UINTN)Buffer > MAX_UINTN - Bytes) {
    return FALSE;
  }
  return (BOOLEAN)((UINTN)Buffer + Bytes <= (UINTN)Context->Pixels ||
         (UINTN)Context->Pixels + Context->MappedBytes <= (UINTN)Buffer);
}

STATIC
UINT32
Encode (IN CONST OPPO_FB_CONTEXT *Context, IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel)
{
  return ((UINT32)Pixel.Red << Context->Layout.RedShift) |
         ((UINT32)Pixel.Green << 8) | ((UINT32)Pixel.Blue << Context->Layout.BlueShift) |
         (Context->Layout.OpaqueAlpha ? 0xff000000U : 0);
}

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
  )
{
  UINTN Row, Column, Stride, SourceRow, DestinationRow;
  UINT32 Fill = 0;
  if (!ContextValid (Context) || Width == 0 || Height == 0 ||
      Width > Context->Layout.Width || Height > Context->Layout.Height ||
      (UINTN)Operation >= EfiGraphicsOutputBltOperationMax) {
    return EFI_INVALID_PARAMETER;
  }
  if ((Operation != EfiBltVideoToBltBuffer &&
       !RectangleFits (Context, DestinationX, DestinationY, Width, Height)) ||
      ((Operation == EfiBltVideoToBltBuffer || Operation == EfiBltVideoToVideo) &&
       !RectangleFits (Context, SourceX, SourceY, Width, Height))) {
    return EFI_INVALID_PARAMETER;
  }
  if (Operation == EfiBltVideoFill) {
    if (BltBuffer == NULL || BltBytes < sizeof (*BltBuffer) ||
        (UINTN)BltBuffer > MAX_UINTN - sizeof (*BltBuffer)) {
      return EFI_INVALID_PARAMETER;
    }
    Fill = Encode (Context, *BltBuffer);
  } else if (Operation != EfiBltVideoToVideo) {
    if (Delta == 0) {
      Delta = Width * 4;
    }
    if (!BufferFits (Context, BltBuffer, BltBytes,
                     Operation == EfiBltBufferToVideo ? SourceX : DestinationX,
                     Operation == EfiBltBufferToVideo ? SourceY : DestinationY,
                     Width, Height, Delta)) {
      return EFI_INVALID_PARAMETER;
    }
  }
  Stride = Context->Layout.StrideBytes / 4;
  for (Row = 0; Row < Height; Row++) {
    UINTN Index = Operation == EfiBltVideoToVideo && DestinationY > SourceY ? Height - 1 - Row : Row;
    // Buffer coordinates are validated only for operations that use them.
    SourceRow = Operation == EfiBltVideoToVideo || Operation == EfiBltVideoToBltBuffer ?
                (SourceY + Index) * Stride : 0;
    DestinationRow = Operation != EfiBltVideoToBltBuffer ? (DestinationY + Index) * Stride : 0;
    for (Column = 0; Column < Width; Column++) {
      UINTN X = Operation == EfiBltVideoToVideo && DestinationX > SourceX ? Width - 1 - Column : Column;
      if (Operation == EfiBltVideoFill) {
        Context->Pixels[DestinationRow + DestinationX + X] = Fill;
      } else if (Operation == EfiBltVideoToVideo) {
        Context->Pixels[DestinationRow + DestinationX + X] = Context->Pixels[SourceRow + SourceX + X];
      } else if (Operation == EfiBltBufferToVideo) {
        CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixel = (CONST VOID *)((CONST UINT8 *)BltBuffer +
                                                (SourceY + Index) * Delta + (SourceX + X) * 4);
        Context->Pixels[DestinationRow + DestinationX + X] = Encode (Context, *Pixel);
      } else {
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixel = (VOID *)((UINT8 *)BltBuffer +
                                                (DestinationY + Index) * Delta + (DestinationX + X) * 4);
        UINT32 Raw = Context->Pixels[SourceRow + SourceX + X];
        Pixel->Red = (UINT8)(Raw >> Context->Layout.RedShift);
        Pixel->Green = (UINT8)(Raw >> 8);
        Pixel->Blue = (UINT8)(Raw >> Context->Layout.BlueShift);
        Pixel->Reserved = 0;
      }
    }
  }
  return EFI_SUCCESS;
}
