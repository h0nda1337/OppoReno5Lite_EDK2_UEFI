/* SPDX-License-Identifier: BSD-2-Clause */
/* Original synchronous SD reader. Board clocks, pads and rails are separate. */
#ifndef OPPO_MSDC_LIB_H
#define OPPO_MSDC_LIB_H
#include <Uefi.h>
typedef struct {
  VOID *Context;
  UINT32 (*Read32)(VOID *Context, UINT32 Offset);
  VOID (*Write32)(VOID *Context, UINT32 Offset, UINT32 Value);
  VOID (*DelayUs)(VOID *Context, UINT32 Microseconds);
} OPPO_MSDC_IO;
typedef struct {
  OPPO_MSDC_IO Io;
  UINT64 Blocks;
  UINT32 Rca;
  UINT32 LastCommand, LastInterrupt;
  UINT32 InitialIocon, Cmd8Attempts;
  EFI_STATUS Cmd8FirstStatus;
  BOOLEAN Ready, BlockAddressed;
} OPPO_MSDC;
EFI_STATUS OppoMsdcInitialize (OPPO_MSDC *Card, CONST OPPO_MSDC_IO *Io);
EFI_STATUS OppoMsdcRead (OPPO_MSDC *Card, UINT64 Lba, UINTN Blocks, VOID *Buffer);
#endif
