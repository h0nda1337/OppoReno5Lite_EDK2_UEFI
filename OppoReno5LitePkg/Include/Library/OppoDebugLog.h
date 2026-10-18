/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef OPPO_DEBUG_LOG_H
#define OPPO_DEBUG_LOG_H
#include <Uefi.h>
/* Call only after the native framebuffer and the entire pool were validated.
 * Initializes one reserved RAM page; no persistent storage is accessed. */
BOOLEAN EFIAPI OppoDebugStart (VOID);
#define OPPO_BOOT_BEFORE_LOAD 1U
#define OPPO_BOOT_LOAD_RETURNED 2U
#define OPPO_BOOT_START_ACTIVE 3U
#define OPPO_BOOT_START_RETURNED 4U
typedef struct {
  UINT32 Stage;
  EFI_STATUS LoadStatus, StartStatus, MapStatus;
  UINT64 ConventionalBytes;
} OPPO_BOOT_FLOW;
/* Stores a diagnostic copy in the reserved log page, with no persistence. */
VOID EFIAPI OppoDebugRecordBootFlow (CONST OPPO_BOOT_FLOW *Flow);
#endif
