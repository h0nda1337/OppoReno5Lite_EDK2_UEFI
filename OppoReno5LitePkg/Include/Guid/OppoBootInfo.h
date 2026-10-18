/** @file
  Original SEC-to-DXE handoff, without private device identifiers.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_BOOT_INFO_H
#define OPPO_BOOT_INFO_H
#include <Uefi.h>

#define OPPO_BOOT_INFO_SIGNATURE 0x4942504fU
#define OPPO_BOOT_INFO_REVISION 1
#define OPPO_BOOT_MEMORY_READY 1U
#define OPPO_BOOT_EXPECTED_DISPLAY_PROFILE 2U
#define OPPO_BOOT_WATCHDOG_STOPPED 4U
#define OPPO_BOOT_SD_PROFILE_VALIDATED 8U
#define OPPO_BOOT_SD_PAD_BANK_VALIDATED 16U
#define OPPO_BOOT_SD_RM_ROLE_VALIDATED 32U
#define OPPO_BOOT_SD_POWER_PROFILE_VALIDATED 64U

typedef struct {
  UINT32 Signature;
  UINT32 Revision;
  UINT64 InitialEl;
  UINT64 CounterFrequency;
  UINT64 FramebufferBase;
  UINT64 FramebufferBytes;
  UINT32 Width;
  UINT32 Height;
  UINT32 Stride;
  UINT8 RedShift;
  UINT8 BlueShift;
  BOOLEAN OpaqueAlpha;
  UINT8 Reserved;
  UINT64 InitialSctlr;
  UINT32 PsciVersion;
  UINT32 Flags;
} OPPO_BOOT_INFO;

extern EFI_GUID gOppoBootInfoHobGuid;
extern EFI_GUID gOppoGopDeviceGuid;
#endif
