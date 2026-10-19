/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef OPPO_DISPLAY_PROBE_H
#define OPPO_DISPLAY_PROBE_H
#include <Library/OppoFdtLib.h>

typedef EFI_STATUS (EFIAPI *OPPO_DISPLAY_READ32)(VOID *Context, UINT64 Address, UINT32 *Value);
typedef EFI_STATUS (EFIAPI *OPPO_DISPLAY_WRITE32)(VOID *Context, UINT64 Address, UINT32 Value);
typedef struct { UINT32 Enabled, Control, Address, Size, Offset, Pitch; } OPPO_DISPLAY_LAYER;
typedef struct {
  UINT32 Base, ClockOff, Enable, Sources, Extended, DataPath, Roi;
  OPPO_DISPLAY_LAYER Layers[7]; /* Four physical slots followed by three extended slots. */
} OPPO_DISPLAY_BANK;
typedef struct { UINT32 ClockGate; OPPO_DISPLAY_BANK Banks[3]; } OPPO_DISPLAY_SNAPSHOT;

/* No register writes. All live FDT bindings must validate before the first read.
 * Gated banks and disabled layers are skipped. No buffer addresses are dereferenced. */
EFI_STATUS OppoDisplayReadSnapshot (CONST OPPO_FDT_VIEW *View, OPPO_DISPLAY_READ32 Read,
                                    VOID *Context, OPPO_DISPLAY_SNAPSHOT *Output);

typedef struct { BOOLEAN Attempted; UINT32 Before, After; } OPPO_DISPLAY_CLEAN_RESULT;
/* Optional SD boot handoff only. Revalidates all bindings and the observed
 * two-surface configuration. Changes only OVL0 SRC_CON bit1, preserving its LC
 * input and the entire OVL0_2L GOP configuration; no pixel or storage writes.
 * A failed layout match performs no writes. Physical visual effect is unproven. */
EFI_STATUS OppoDisplayHideInherited (CONST OPPO_FDT_VIEW *View, OPPO_DISPLAY_READ32 Read,
                                     OPPO_DISPLAY_WRITE32 Write, VOID *Context,
                                     OPPO_DISPLAY_CLEAN_RESULT *Result);
#endif
