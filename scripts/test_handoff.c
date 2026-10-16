/* SPDX-License-Identifier: BSD-2-Clause */
#include <Library/OppoHandoffLib.h>
#include <Library/OppoFdtLib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static UINT8 *load(const char *path, UINTN *size)
{
  FILE *f = fopen(path, "rb");
  assert(f && fseek(f, 0, SEEK_END) == 0);
  long n = ftell(f);
  assert(n >= 40 && n <= 2 * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0);
  UINT8 *p = malloc((size_t)n);
  assert(p && fread(p, 1, (size_t)n, f) == (size_t)n);
  fclose(f); *size = (UINTN)n; return p;
}

static void fixture_checks(UINT8 *data, UINTN size)
{
  OPPO_BOOT_HANDOFF info, before;
  OPPO_FDT_VIEW view;
  CONST VOID *value;
  UINT32 bytes;
  UINT8 *changed = malloc(size);
  assert(changed && OppoInspectHandoff(data, size, 0x57300000, &info) == EFI_SUCCESS);
  assert(info.RamCount == 1 && info.Ram[0].Base == 0x40000000 && info.Ram[0].Length == 0x180000000ULL);
  assert(info.ReservedCount == 5 && info.DynamicReservationCount == 1 && info.UnresolvedReservationCount == 1);
  assert(info.FdtPhysicalAddressRecorded && info.PsciSmcDeclared);
  assert(info.NativeFramebufferPresent && info.NativePanelFound && info.NativeFramebufferCoveredByReservation);
  assert(info.NativeFramebuffer.Base == 0x90000000 && info.NativeFramebuffer.Length == 0x2000000);
  assert(info.SplitFramebufferPresent && info.FramebufferDeclarationsDisagree);
  assert(OppoLoadRegionIsUsable(TRUE, 0x43000000, 0x4000000, info.Ram, info.RamCount,
                               info.Reserved, info.ReservedCount));
  assert(!OppoLoadRegionIsUsable(TRUE, 0x78800000, 4096, info.Ram, info.RamCount,
                                info.Reserved, info.ReservedCount));
  assert(!OppoLoadRegionIsUsable(TRUE, 0x57300000, 4096, info.Ram, info.RamCount,
                                info.Reserved, info.ReservedCount));
  before = info;
  assert(OppoInspectHandoff(data, size, 0x200000000ULL, &info) == EFI_COMPROMISED_DATA);
  assert(memcmp(&info, &before, sizeof info) == 0);
  assert(OppoInspectHandoff(data, size, 0x57300001, &info) == EFI_COMPROMISED_DATA);
  assert(memcmp(&info, &before, sizeof info) == 0);
  for (UINTN i = 0; i < 4; i++) {
    memcpy(changed, data, size);
    assert(OppoFdtOpen(changed, size, &view));
    assert(OppoFdtProperty(&view, i == 0 || i == 3 ? "/memory@40000000" : i == 1 ? "/psci" : "/chosen",
                           i == 0 ? "reg" : i == 1 ? "method" : i == 2 ? "atag,videolfb" : "status",
                           &value, &bytes) == EFI_SUCCESS);
    UINT8 *p = changed + ((CONST UINT8 *)value - changed);
    if (i == 0) memset(p + 8, 0, 8); // Zero RAM length.
    if (i == 1) memcpy(p, "hvc", 4); // Unsupported for this MT6779 handoff.
    if (i == 2) memset(p, 0xff, 8); // Framebuffer address would overflow.
    if (i == 3) memcpy(p, "fail", 5); // Unavailable memory must not become a usable bank.
    assert(OppoInspectHandoff(changed, size, 0, &info) != EFI_SUCCESS);
    assert(memcmp(&info, &before, sizeof info) == 0);
  }
  UINT32 random = 0x196c7305;
  for (UINTN i = 0; i < 512; i++) {
    memcpy(changed, data, size);
    random ^= random << 13; random ^= random >> 17; random ^= random << 5;
    changed[random % size] ^= (UINT8)(1U << (i % 8));
    info = before;
    EFI_STATUS status = OppoInspectHandoff(changed, size, 0, &info);
    if (status != EFI_SUCCESS) assert(memcmp(&info, &before, sizeof info) == 0);
    else assert(info.RamCount > 0 && info.RamCount <= OPPO_MAX_RAM_BANKS &&
                info.ReservedCount <= OPPO_MAX_BOOT_RESERVATIONS);
  }
  free(changed);
}

int main(int argc, char **argv)
{
  UINTN size;
  assert(argc == 2 || argc == 3);
  UINT8 *fixture = load(argv[1], &size);
  fixture_checks(fixture, size);
  free(fixture);
  if (argc == 3) {
    OPPO_BOOT_HANDOFF info;
    UINT8 *raw = load(argv[2], &size);
    assert(OppoInspectHandoff(raw, size, 0, &info) == EFI_SUCCESS);
    assert(info.RamCount == 1 && info.Ram[0].Length == 0x180000000ULL);
    assert(info.NativeFramebuffer.Base == 0x76520000 && info.NativeFramebuffer.Length == 0x22e0000);
    assert(info.NativeFramebufferCoveredByReservation && info.FramebufferDeclarationsDisagree && info.PsciSmcDeclared);
    printf("Private runtime handoff: %zu RAM bank, %zu explicit reservations, %u dynamic and %u unresolved.\n",
           (size_t)info.RamCount, (size_t)info.ReservedCount,
           info.DynamicReservationCount, info.UnresolvedReservationCount);
    free(raw);
  }
  puts("Original handoff interpretation, reservation exclusion and 512 corruptions passed with host sanitizers; no hardware operation.");
  return 0;
}
