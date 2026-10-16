/* SPDX-License-Identifier: BSD-2-Clause */
#include <Library/OppoFrameBufferLib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void overlap_rectangles(void)
{
  UINT32 memory[45], expected[45], original[45];
  OPPO_FB_LAYOUT layout = {7, 5, 36, 0, 16, TRUE};
  OPPO_FB_CONTEXT context;
  assert(OppoFbInitialize(&context, memory, sizeof memory, &layout));
  for (UINTN i = 0; i < 45; i++) original[i] = (UINT32)(i * 0x1020304);
  // Compare all valid 2D rectangle moves with an immutable source snapshot.
  for (UINTN width = 1; width <= 7; width++)
    for (UINTN height = 1; height <= 5; height++)
      for (UINTN sy = 0; sy + height <= 5; sy++)
        for (UINTN sx = 0; sx + width <= 7; sx++)
          for (UINTN dy = 0; dy + height <= 5; dy++)
            for (UINTN dx = 0; dx + width <= 7; dx++) {
              memcpy(memory, original, sizeof memory);
              memcpy(expected, original, sizeof expected);
              for (UINTN y = 0; y < height; y++)
                for (UINTN x = 0; x < width; x++)
                  expected[(dy + y) * 9 + dx + x] = original[(sy + y) * 9 + sx + x];
              assert(OppoFbBlt(&context, NULL, 0, EfiBltVideoToVideo,
                              sx, sy, dx, dy, width, height, 0) == EFI_SUCCESS);
              assert(memcmp(memory, expected, sizeof memory) == 0);
            }
}

static void color_and_bounds(void)
{
  UINT32 guarded[22], before[22];
  OPPO_FB_CONTEXT context, invalid;
  OPPO_FB_LAYOUT layout = {4, 3, 24, 0, 16, TRUE};
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL color = {0x56, 0x34, 0x12, 0};
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL input[9], output[9];
  for (UINTN i = 0; i < 22; i++) guarded[i] = 0xabcdef01;
  assert(OppoFbInitialize(&context, guarded + 2, 18 * 4, &layout));
  assert(OppoFbBlt(&context, &color, sizeof color, EfiBltVideoFill,
                  MAX_UINTN, MAX_UINTN, 0, 0, 4, 3, 0) == EFI_SUCCESS);
  for (UINTN i = 0; i < 22; i++) {
    BOOLEAN visible = i >= 2 && i < 20 && (i - 2) % 6 < 4;
    assert(guarded[i] == (visible ? 0xff563412 : 0xabcdef01));
  }
  memset(output, 0xa5, sizeof output);
  assert(OppoFbBlt(&context, output, sizeof output, EfiBltVideoToBltBuffer,
                  1, 1, 1, 1, 2, 2, 12) == EFI_SUCCESS);
  assert(output[4].Red == 0x12 && output[4].Green == 0x34 && output[4].Blue == 0x56);
  assert(output[4].Reserved == 0 && output[8].Reserved == 0);
  assert(output[0].Red == 0xa5 && output[6].Red == 0xa5);
  assert(OppoFbBlt(&context, output, sizeof output, EfiBltVideoToBltBuffer,
                  0, 0, 0, 0, 2, 2, 0) == EFI_SUCCESS);
  assert(output[3].Red == 0x12 && output[3].Reserved == 0);
  assert(OppoFbBlt(&context, output, 16, EfiBltBufferToVideo,
                  0, 0, 2, 1, 2, 2, 0) == EFI_SUCCESS);
  for (UINTN i = 0; i < 9; i++)
    input[i] = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){(UINT8)i, (UINT8)(i + 1), (UINT8)(i + 2), 0};
  assert(OppoFbBlt(&context, input, sizeof input, EfiBltBufferToVideo,
                  1, 1, 0, 0, 2, 2, 12) == EFI_SUCCESS);
  assert(guarded[2] == 0xff040506 && guarded[9] == 0xff08090a);
  memcpy(before, guarded, sizeof before);
  assert(OppoFbBlt(&context, input, sizeof input - 1, EfiBltBufferToVideo,
                  1, 1, 0, 0, 2, 2, 12) == EFI_INVALID_PARAMETER);
  assert(OppoFbBlt(&context, input, sizeof input, EfiBltBufferToVideo,
                  MAX_UINTN, 0, 0, 0, 2, 2, 12) == EFI_INVALID_PARAMETER);
  assert(OppoFbBlt(&context, input, sizeof input, EfiBltVideoToBltBuffer,
                  0, 0, 0, MAX_UINTN, 2, 2, 12) == EFI_INVALID_PARAMETER);
  assert(OppoFbBlt(&context, input, sizeof input, EfiBltBufferToVideo,
                  0, 1, 0, 0, 2, 2, MAX_UINTN - 3) == EFI_INVALID_PARAMETER);
  assert(OppoFbBlt(&context, input, sizeof input, EfiBltBufferToVideo,
                  0, 0, 0, 0, 2, 2, 7) == EFI_INVALID_PARAMETER);
  assert(OppoFbBlt(&context, input, sizeof input, EfiBltVideoFill,
                  0, 0, 3, 0, 2, 1, 0) == EFI_INVALID_PARAMETER);
  assert(OppoFbBlt(&context, input, sizeof input, EfiBltVideoFill,
                  0, 0, 0, 0, 0, 1, 0) == EFI_INVALID_PARAMETER);
  assert(OppoFbBlt(&context, input, sizeof input, (EFI_GRAPHICS_OUTPUT_BLT_OPERATION)-1,
                  0, 0, 0, 0, 1, 1, 0) == EFI_INVALID_PARAMETER);
  assert(OppoFbBlt(&context, (VOID *)(guarded + 2), 4, EfiBltBufferToVideo,
                  0, 0, 0, 0, 1, 1, 0) == EFI_INVALID_PARAMETER);
  assert(memcmp(before, guarded, sizeof before) == 0);
  assert(OppoFbBlt(&context, (VOID *)(MAX_UINTN - 1), 4, EfiBltVideoFill,
                  0, 0, 0, 0, 1, 1, 0) == EFI_INVALID_PARAMETER);
  invalid = context;
  invalid.Layout.StrideBytes = 1;
  assert(OppoFbBlt(&invalid, &color, sizeof color, EfiBltVideoFill,
                  0, 0, 0, 0, 1, 1, 0) == EFI_INVALID_PARAMETER);
  assert(!OppoFbInitialize(&invalid, guarded + 2, 71, &layout));
  assert(!OppoFbInitialize(&invalid, (UINT8 *)guarded + 1, 72, &layout));
  layout.BlueShift = 0;
  assert(!OppoFbInitialize(&invalid, guarded + 2, 72, &layout));
  layout.RedShift = 16;
  assert(OppoFbInitialize(&context, guarded + 2, 72, &layout));
  assert(OppoFbBlt(&context, &color, sizeof color, EfiBltVideoFill,
                  0, 0, 0, 0, 1, 1, 0) == EFI_SUCCESS);
  assert(guarded[2] == 0xff123456);
}

static void measured_geometry(void)
{
  // Synthetic host storage matching the measured pitch; no device memory.
  const UINTN size = 31334400;
  UINT32 *pixels = calloc(1, size);
  OPPO_FB_LAYOUT layout = {1080, 2400, 4352, 0, 16, TRUE};
  OPPO_FB_CONTEXT context;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL white = {255, 255, 255, 0};
  assert(pixels && OppoFbInitialize(&context, pixels, size, &layout));
  assert(OppoFbBlt(&context, &white, sizeof white, EfiBltVideoFill,
                  0, 0, 1079, 2399, 1, 1, 0) == EFI_SUCCESS);
  assert(pixels[2399 * 1088 + 1079] == 0xffffffff);
  assert(pixels[2399 * 1088 + 1080] == 0);
  free(pixels);
}

int main(void)
{
  color_and_bounds();
  overlap_rectangles();
  measured_geometry();
  puts("Four BLT operations, all small overlap rectangles, color order, alpha, pitch and bounds passed on host.");
  return 0;
}
