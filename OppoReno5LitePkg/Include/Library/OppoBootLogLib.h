/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef OPPO_BOOT_LOG_LIB_H
#define OPPO_BOOT_LOG_LIB_H
#include <Uefi.h>
/* Physical display access only before ExitBootServices, after SEC validation.
 * No allocations or Boot Services. Caller must exclude concurrent writers. */
VOID EFIAPI OppoBootLogReset (EFI_RESET_TYPE Type, EFI_STATUS Status);
#endif
