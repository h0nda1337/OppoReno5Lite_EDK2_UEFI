/* SPDX-License-Identifier: BSD-2-Clause */
#include <Library/OppoAcpiLib.h>
#include <Library/BaseMemoryLib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

VOID *EFIAPI ZeroMem(VOID *p, UINTN n) { return memset(p, 0, n); }
VOID *EFIAPI CopyMem(VOID *d, CONST VOID *s, UINTN n) { return memmove(d, s, n); }

// A SYNTHETIC mapping inside the captured region, not an observation of GICR.
static OPPO_ACPI_PROFILE example(void)
{
  OPPO_ACPI_PROFILE p = {0};
  for (UINTN i = 0; i < 8; i++) {
    p.Cpu[i].Mpidr = i << 8;
    p.Cpu[i].RedistributorBase = 0x0c040000 + i * 0x20000;
  }
  return p;
}

static _Alignas(16) UINT8 destination[4096 + 32];
static UINT8 before[sizeof(destination)];
static OPPO_ACPI_LAYOUT layout, saved;

static void rejected(OPPO_ACPI_PROFILE *p, UINTN bytes, UINT64 physical, EFI_STATUS expected)
{
  memset(destination, 0xa5, sizeof(destination));
  memcpy(before, destination, sizeof(before));
  memset(&layout, 0x3c, sizeof(layout)); saved = layout;
  assert(OppoBuildAcpiTables(p, destination + 16, bytes, physical, &layout) == expected);
  assert(memcmp(before, destination, sizeof(before)) == 0);
  assert(memcmp(&layout, &saved, sizeof(layout)) == 0);
}

static void save_file(const char *name, const void *p, UINTN n)
{
  char path[128];
  assert(snprintf(path, sizeof(path), ".build/acpi/%s", name) > 0);
  FILE *f = fopen(path, "wb");
  assert(f && fwrite(p, 1, n, f) == n && fclose(f) == 0);
}

static void accepted(OPPO_ACPI_PROFILE *p, UINT64 physical, const char *name, int tables)
{
  UINTN n = OppoAcpiTableBytes();
  memset(destination, 0xa5, sizeof(destination));
  assert(OppoBuildAcpiTables(p, destination + 16, n, physical, &layout) == EFI_SUCCESS);
  assert(layout.TotalBytes == n && n <= 4096);
  for (UINTN i = 0; i < 16; i++) assert(destination[i] == 0xa5);
  for (UINTN i = n + 16; i < sizeof(destination); i++) assert(destination[i] == 0xa5);
  save_file(name, destination + 16, n);
  if (tables) {
    struct { const char *name; UINT32 offset; } parts[] = {
      {"generated-dsdt.aml", layout.DsdtOffset}, {"fadt.dat", layout.FadtOffset},
      {"madt.dat", layout.MadtOffset}, {"gtdt.dat", layout.GtdtOffset}
    };
    for (UINTN i = 0; i < 4; i++) {
      UINT32 length;
      memcpy(&length, destination + 16 + parts[i].offset + 4, 4);
      assert(length >= 36 && length <= n - parts[i].offset);
      save_file(parts[i].name, destination + 16 + parts[i].offset, length);
    }
  }
}

int main(void)
{
  UINTN n = OppoAcpiTableBytes();
  OPPO_ACPI_PROFILE p = example(), bad;
  assert(n > 1024 && n <= 4096);
  for (UINTN bytes = 0; bytes < n; bytes++)
    rejected(&p, bytes, 0x90000000, EFI_BUFFER_TOO_SMALL);
  rejected(NULL, n, 0x90000000, EFI_INVALID_PARAMETER);
  rejected(&p, n, 0, EFI_INVALID_PARAMETER);
  rejected(&p, n, 0x90000008, EFI_INVALID_PARAMETER);
  rejected(&p, n, MAX_UINT64 & ~15ULL, EFI_INVALID_PARAMETER);
  assert(OppoBuildAcpiTables(&p, NULL, n, 0x90000000, &layout) == EFI_INVALID_PARAMETER);
  assert(OppoBuildAcpiTables(&p, destination + 17, n, 0x90000000, &layout) == EFI_INVALID_PARAMETER);
  assert(OppoBuildAcpiTables(&p, destination + 16, n, 0x90000000, NULL) == EFI_INVALID_PARAMETER);
  memset(destination, 0xa5, sizeof(destination)); memcpy(before, destination, sizeof(before));
  assert(OppoBuildAcpiTables(&p, destination + 16, n, 0x90000000,
    (OPPO_ACPI_LAYOUT *)(destination + 24)) == EFI_INVALID_PARAMETER);
  assert(memcmp(before, destination, sizeof(before)) == 0);
  assert(OppoBuildAcpiTables((OPPO_ACPI_PROFILE *)(destination + 16), destination + 16,
    n, 0x90000000, &layout) == EFI_INVALID_PARAMETER);
  assert(memcmp(before, destination, sizeof(before)) == 0);
  OPPO_ACPI_PROFILE original = p;
  assert(OppoBuildAcpiTables(&p, destination + 16, n, 0x90000000,
    (OPPO_ACPI_LAYOUT *)&p) == EFI_INVALID_PARAMETER);
  assert(memcmp(&p, &original, sizeof(p)) == 0);

  bad = p; bad.BootMpidr = 0x100; rejected(&bad, n, 0x90000000, EFI_UNSUPPORTED);
  bad = p; bad.BootMpidr = bad.Cpu[0].Mpidr = BIT31;
  rejected(&bad, n, 0x90000000, EFI_UNSUPPORTED);
  for (UINTN i = 0; i < 8; i++) {
    bad = p; bad.Cpu[i].Mpidr = 0x800; rejected(&bad, n, 0x90000000, EFI_UNSUPPORTED);
    bad = p; bad.Cpu[i].Mpidr = i == 0 ? 0x100 : 0;
    rejected(&bad, n, 0x90000000, EFI_UNSUPPORTED);
    UINT64 invalid[] = {0, 0x0c03ffff, 0x0c030000, 0x0c040001, 0x0c230000,
                        0x0c240000, MAX_UINT64 & ~0xffffULL};
    for (UINTN j = 0; j < sizeof(invalid)/sizeof(invalid[0]); j++) {
      bad = p; bad.Cpu[i].RedistributorBase = invalid[j];
      rejected(&bad, n, 0x90000000, EFI_UNSUPPORTED);
    }
    if (i) {
      bad = p; bad.Cpu[i].RedistributorBase = p.Cpu[0].RedistributorBase;
      rejected(&bad, n, 0x90000000, EFI_UNSUPPORTED);
      bad.Cpu[i].RedistributorBase += 0x10000; // distinct, but overlapping frame.
      rejected(&bad, n, 0x90000000, EFI_UNSUPPORTED);
    }
  }
  accepted(&p, 0x90000000, "low.bin", 1);
  accepted(&p, 0x100000000ULL, "high.bin", 0);
  // Last valid frame boundary is accepted. Order is supplied, never inferred.
  p.Cpu[7].RedistributorBase = 0x0c220000;
  accepted(&p, 0x90000000, "last-frame.bin", 0);
  p = example();
  OPPO_ACPI_CPU swap = p.Cpu[0]; p.Cpu[0] = p.Cpu[6]; p.Cpu[6] = swap;
  p.BootMpidr = 0x600;
  // Also break the affinity/address-order correspondence in this synthetic map.
  UINT64 base = p.Cpu[2].RedistributorBase;
  p.Cpu[2].RedistributorBase = p.Cpu[5].RedistributorBase; p.Cpu[5].RedistributorBase = base;
  accepted(&p, 0x90000000, "shuffled.bin", 0);
  puts("ACPI host: bounded writes, every short buffer, invalid/overlapping profiles, boot ordering and 64-bit pointers passed");
  return 0;
}
