/* SPDX-License-Identifier: BSD-2-Clause */
#include <assert.h>
#include <Library/OppoPlatformLib.h>

int main (void)
{
  OPPO_MEMORY_RANGE Banks[] = {{0x40000000, 0x10000000}};
  OPPO_MEMORY_RANGE Reserved[] = {{0x48000000, 0x200000}};
  OPPO_MEMORY_RANGE Invalid[] = {{MAX_UINT64 - 1, 8}};
  CONST OPPO_SOC_DESCRIPTION *Soc = OppoGetSocDescription ();

  assert (Soc->CpuCount == 8 && Soc->GicDistributor == 0x0C000000);
  assert (Soc->GicRedistributors == 0x0C040000 && Soc->Watchdog == 0x10007000);
  assert (!OppoLoadRegionIsUsable (FALSE, 0x41000000, 0x1000, Banks, 1, Reserved, 1));
  assert (OppoLoadRegionIsUsable (TRUE, 0x41000000, 0x1000, Banks, 1, Reserved, 1));
  assert (!OppoLoadRegionIsUsable (TRUE, 0x47FFF000, 0x2000, Banks, 1, Reserved, 1));
  assert (OppoLoadRegionIsUsable (TRUE, 0x47FFF000, 0x1000, Banks, 1, Reserved, 1));
  assert (OppoLoadRegionIsUsable (TRUE, 0x48200000, 0x1000, Banks, 1, Reserved, 1));
  assert (!OppoLoadRegionIsUsable (TRUE, 0x4FFFF000, 0x2000, Banks, 1, Reserved, 1));
  assert (!OppoLoadRegionIsUsable (TRUE, MAX_UINT64 - 1, 8, Banks, 1, Reserved, 1));
  assert (!OppoLoadRegionIsUsable (TRUE, 0x41000000, 0, Banks, 1, Reserved, 1));
  assert (!OppoLoadRegionIsUsable (TRUE, 0x41000000, 0x1000, Invalid, 1, Reserved, 1));
  assert (!OppoLoadRegionIsUsable (TRUE, 0x41000000, 0x1000, Banks, 1, Invalid, 1));
  assert (!OppoLoadRegionIsUsable (TRUE, 0x41000000, 0x1000, Banks, 1, NULL, 1));
  assert (!OppoLoadRegionIsUsable (TRUE, 0x41000000, 0x1000, NULL, 0, NULL, 0));
  return 0;
}
