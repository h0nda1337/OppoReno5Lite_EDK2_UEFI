/* SPDX-License-Identifier: BSD-2-Clause */
#include <Library/OppoMemoryPlanLib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static OPPO_BOOT_HANDOFF fixture(void)
{
  OPPO_BOOT_HANDOFF info = {0};
  info.Ram[0] = (OPPO_MEMORY_RANGE){0x40000000, 0x180000000ULL}; info.RamCount = 1;
  info.Reserved[0] = (OPPO_MEMORY_RANGE){0x78800000, 0x5800000};
  info.ReservedFlags[0] = OPPO_RESERVATION_NO_MAP;
  info.Reserved[1] = (OPPO_MEMORY_RANGE){0x57300000, 0x3a615};
  info.ReservedFlags[1] = OPPO_RESERVATION_BOOT_DATA;
  info.ReservedCount = 2; info.FdtPhysicalAddressRecorded = TRUE;
  info.DynamicReservationCount = 6; info.EmptyReservationNodeCount = 1;
  return info;
}

static void synthetic(void)
{
  OPPO_BOOT_HANDOFF info = fixture(), changed;
  OPPO_EARLY_MEMORY_PLAN plan, before;
  OPPO_MEMORY_RANGE image = {0x40080000, 0x800000}, pool = {0x43000000, 0x4000000};
  assert(OppoPlanEarlyMemory(&info, image, pool, &plan) == EFI_SUCCESS);
  assert(plan.ProtectedCount == 3 && plan.DynamicReservationsDeferredToOs == 6 && plan.EmptyNodesIgnored == 1);
  assert(plan.Protected[1].Base == 0x57300000 && plan.Protected[1].Length == 0x3b000);
  assert(plan.ProtectedType[0] == EfiReservedMemoryType && plan.ProtectedType[1] == EfiBootServicesData);
  assert(plan.ProtectedType[2] == EfiBootServicesCode);
  before = plan;
  for (UINTN i = 0; i < 7; i++) {
    changed = info;
    if (i == 0) changed.UnresolvedReservationCount = 1;
    if (i == 1) changed.FdtPhysicalAddressRecorded = FALSE;
    if (i == 2) changed.Reserved[0] = pool;
    if (i == 3) changed.Reserved[1].Base = MAX_UINT64 - 4096;
    if (i == 4) changed.ReservedCount = OPPO_MAX_BOOT_RESERVATIONS + 1;
    if (i == 5) changed.Reserved[1].Base = changed.Reserved[0].Base + 1;
    if (i == 6) changed.ReservedFlags[0] = 0x80000000;
    assert(OppoPlanEarlyMemory(&changed, image, pool, &plan) != EFI_SUCCESS);
    assert(memcmp(&plan, &before, sizeof plan) == 0);
  }
  OPPO_MEMORY_RANGE bad[] = {{0x43000001, 0x4000000}, {0x43000000, 4096}, {0x40080000, 0x4000000},
                            {0x200000000ULL, 0x4000000}, {MAX_UINT64 - 4095, 0x4000000}};
  for (UINTN i = 0; i < sizeof bad / sizeof bad[0]; i++) {
    assert(OppoPlanEarlyMemory(&info, image, bad[i], &plan) != EFI_SUCCESS);
    assert(memcmp(&plan, &before, sizeof plan) == 0);
  }
  // Page rounding, not raw byte adjacency, defines the exclusion boundary.
  changed = info;
  changed.Reserved[2] = (OPPO_MEMORY_RANGE){0x5733a616, 1}; changed.ReservedCount = 3;
  assert(OppoPlanEarlyMemory(&changed, image, pool, &plan) == EFI_COMPROMISED_DATA);
}

static void private_capture(const char *path)
{
  FILE *f = fopen(path, "rb");
  assert(f && fseek(f, 0, SEEK_END) == 0);
  long bytes = ftell(f);
  assert(bytes >= 40 && bytes <= 2 * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0);
  UINT8 *raw = malloc((size_t)bytes);
  assert(raw && fread(raw, 1, (size_t)bytes, f) == (size_t)bytes); fclose(f);
  OPPO_BOOT_HANDOFF info;
  OPPO_EARLY_MEMORY_PLAN plan;
  // This is a supplied address for offline planning, not a measured entry x0.
  assert(OppoInspectHandoff(raw, (UINTN)bytes, 0x57300000, &info) == EFI_SUCCESS);
  assert(OppoPlanEarlyMemory(&info, (OPPO_MEMORY_RANGE){0x40080000, 0x800000},
                            (OPPO_MEMORY_RANGE){0x43000000, 0x4000000}, &plan) == EFI_SUCCESS);
  assert(plan.DynamicReservationsDeferredToOs == 6 && plan.EmptyNodesIgnored == 1);
  printf("Offline capture-based proposal: 64 MiB pool, 8 MiB image, %zu protected ranges; no physical allocation.\n",
         (size_t)plan.ProtectedCount);
  free(raw);
}

int main(int argc, char **argv)
{
  assert(argc == 1 || argc == 2);
  synthetic();
  if (argc == 2) private_capture(argv[1]);
  puts("Early memory policy, page rounding, exclusions, deferred dynamic requests and unchanged failure output passed with host sanitizers.");
  return 0;
}
