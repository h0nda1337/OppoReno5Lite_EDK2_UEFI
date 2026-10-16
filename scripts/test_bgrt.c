/* SPDX-License-Identifier: BSD-2-Clause */
#include <Library/OppoBgrtLib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void le32 (UINT8 *p, UINT32 v)
{
  for (int i = 0; i < 4; ++i) p[i] = (UINT8)(v >> (i * 8));
}

static UINT8 sum (const void *p, size_t n)
{
  const UINT8 *b = p;
  UINT8 v = 0;
  for (size_t i = 0; i < n; ++i) v = (UINT8)(v + b[i]);
  return v;
}

static void fixture (UINT8 *bmp)
{
  // Synthetic 2x2 raster, including two padding bytes per row; no OEM asset.
  memset (bmp, 0, 70);
  bmp[0] = 'B'; bmp[1] = 'M';
  le32 (bmp + 2, 70); le32 (bmp + 10, 54); le32 (bmp + 14, 40);
  le32 (bmp + 18, 2); le32 (bmp + 22, 2);
  bmp[26] = 1; bmp[28] = 24;
  le32 (bmp + 34, 16);
}

int main (int argc, char **argv)
{
  UINT8 bmp[70];
  EFI_ACPI_6_6_BOOT_GRAPHICS_RESOURCE_TABLE table, before;
  fixture (bmp);
  assert (OppoPrepareBgrt (&table, bmp, sizeof bmp, 0x88000000, 1080, 2400, 539, 900));
  assert (sizeof table == 56 && table.Header.Length == 56 && sum (&table, sizeof table) == 0);
  assert (table.Status == 0 && table.ImageAddress == 0x88000000 && table.ImageOffsetX == 539);
  assert (OppoBgrtSetDisplayed (&table, TRUE) && table.Status == 1 && sum (&table, sizeof table) == 0);
  assert (OppoBgrtSetDisplayed (&table, FALSE) && table.Status == 0 && sum (&table, sizeof table) == 0);
  before = table;
  table.ImageOffsetY++;
  assert (!OppoBgrtSetDisplayed (&table, TRUE));
  table = before;
  assert (!OppoPrepareBgrt (&table, bmp, 53, 1, 1080, 2400, 0, 0));
  assert (!memcmp (&table, &before, sizeof table));
  assert (!OppoPrepareBgrt (&table, bmp, sizeof bmp, 0, 1080, 2400, 0, 0));
  assert (!OppoPrepareBgrt (&table, bmp, sizeof bmp, MAX_UINT64 - 32, 1080, 2400, 0, 0));
  assert (!OppoPrepareBgrt (&table, bmp, sizeof bmp, 1, 1080, 2400, 1079, 0));
  assert (!OppoPrepareBgrt (&table, bmp, sizeof bmp, 1, 1080, 2400, 0, MAX_UINT32));
  assert (!OppoPrepareBgrt (&table, bmp, sizeof bmp, 1, 1080, 0, 0, 0));
  // Malformed DIB size, signed dimensions, compression, offset and image length.
  const int offsets[] = {14, 18, 22, 30, 10, 34};
  const UINT32 values[] = {124, MAX_UINT32, 0, 1, 69, 15};
  for (size_t i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) {
    fixture (bmp);
    le32 (bmp + offsets[i], values[i]);
    assert (!OppoPrepareBgrt (&table, bmp, sizeof bmp, 1, 1080, 2400, 0, 0));
    assert (!memcmp (&table, &before, sizeof table));
  }
  if (argc == 2) {
    // Optional local check of an owner's BMP; bytes are never copied to the repo.
    FILE *f = fopen (argv[1], "rb");
    assert (f && fseek (f, 0, SEEK_END) == 0);
    long length = ftell (f);
    assert (length > 0 && length <= 32 * 1024 * 1024 && fseek (f, 0, SEEK_SET) == 0);
    UINT8 *actual = malloc ((size_t)length);
    assert (actual && fread (actual, 1, (size_t)length, f) == (size_t)length);
    fclose (f);
    assert (OppoPrepareBgrt (&table, actual, (UINTN)length, 0x88000000, 1080, 2400, 230, 827));
    assert (sum (&table, sizeof table) == 0 && table.Status == 0);
    free (actual);
  }
  puts ("BMP bounds, BGRT checksum and visibility transitions passed on host.");
  return 0;
}
