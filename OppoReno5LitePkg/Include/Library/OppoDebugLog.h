/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef OPPO_DEBUG_LOG_H
#define OPPO_DEBUG_LOG_H
#include <Uefi.h>
/* Call only after the native framebuffer and the entire pool were validated.
 * Initializes one reserved RAM page; no persistent storage is accessed. */
BOOLEAN EFIAPI OppoDebugStart (VOID);
#endif
