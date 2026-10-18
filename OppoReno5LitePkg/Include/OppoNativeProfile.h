/* SPDX-License-Identifier: BSD-2-Clause */
/* Initial laboratory profile. Addresses are checked against this boot's FDT. */
#ifndef OPPO_NATIVE_PROFILE_H
#define OPPO_NATIVE_PROFILE_H
#define OPPO_NATIVE_FD_BASE       0x40080000
#define OPPO_NATIVE_FD_BYTES      0x00800000
#define OPPO_NATIVE_FV_BASE       0x40081000
#define OPPO_NATIVE_FV_BYTES      0x007ff000
#define OPPO_NATIVE_POOL_BASE     0x43000000
#define OPPO_NATIVE_POOL_BYTES    0x04000000
#define OPPO_NATIVE_STACK_BYTES   0x00010000
#define OPPO_NATIVE_DEBUG_BASE    (OPPO_NATIVE_POOL_BASE + OPPO_NATIVE_POOL_BYTES - OPPO_NATIVE_STACK_BYTES - 4096)
#define OPPO_NATIVE_DRAM_BASE     0x40000000
#define OPPO_NATIVE_DRAM_END      0x1c0000000
#define OPPO_NATIVE_FB_BASE       0x76520000
#define OPPO_NATIVE_FB_WIDTH      1080
#define OPPO_NATIVE_FB_HEIGHT     2400
#define OPPO_NATIVE_FB_STRIDE     4352
/* Candidate pre-Linux ordering: the MMU probe's blue was observed red.
 * This differs from the later Linux capture and still needs an RGB check. */
#define OPPO_NATIVE_FB_RED_SHIFT  16
#define OPPO_NATIVE_FB_BLUE_SHIFT 0
#define OPPO_NATIVE_GICD_BASE     0x0c000000
#define OPPO_NATIVE_GICR_BASE     0x0c040000
#define OPPO_NATIVE_WDT_BASE      0x10007000
/* Resolved by compatible role in the captured FDT, not a header comment. */
#define OPPO_NATIVE_SD_PAD_BASE   0x11c20000
#define OPPO_NATIVE_SD_PAD_PATH   "/soc/iocfg_rm@11c20000"
#endif
