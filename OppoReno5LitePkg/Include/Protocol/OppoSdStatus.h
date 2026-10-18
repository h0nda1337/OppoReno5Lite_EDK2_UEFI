/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef OPPO_SD_STATUS_H
#define OPPO_SD_STATUS_H
#include <Uefi.h>
// Public diagnostics contain controller state, never card identifiers.
typedef struct {
  EFI_STATUS Profile, Board, Initialize, Publish;
  UINT32 PadMode, GpioMisc, GpioMode16, GpioMode17, GpioMode1;
  UINT32 LastCommand, LastInterrupt;
  UINT64 Blocks;
  UINT32 InitialIocon, ActiveIocon, Config, Signal, PadTune0;
  UINT32 Cmd8Attempts;
  EFI_STATUS Cmd8FirstStatus;
  UINT32 IesBefore, IesAfter, SmtBefore, SmtAfter, PullDirection, PullR0, PullR1;
} OPPO_SD_STATUS;
extern EFI_GUID gOppoSdStatusProtocolGuid;
extern EFI_GUID gOppoSdDeviceGuid;
#endif
