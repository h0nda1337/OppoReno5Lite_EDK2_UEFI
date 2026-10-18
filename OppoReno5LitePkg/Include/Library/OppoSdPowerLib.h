/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef OPPO_SD_POWER_LIB_H
#define OPPO_SD_POWER_LIB_H
#include <Uefi.h>
// V2 I2C PIO only. Offset zero is an 8-bit FIFO; other accesses are 16-bit.
// The caller validates the board, enables I2C5's clock and supplies delays.
typedef struct {
  VOID *Context;
  UINT32 (*Read)(VOID *Context, UINT32 Offset);
  VOID (*Write)(VOID *Context, UINT32 Offset, UINT32 Value);
  VOID (*Delay)(VOID *Context, UINT32 Us);
} OPPO_I2C_IO;
typedef struct {
  EFI_STATUS Status, Restore;
  UINT32 Valid; // bit0 chip ID, bits1..6 register bytes in the order below.
  UINT32 Interrupt, LastAddress, LastRegister;
  UINT8 ChipId, VmcControl1, VmcControl2, VmcVoltage;
  UINT8 VmchControl1, VmchControl2, VmchVoltage;
  UINT32 ClockSelect, ClockGate, Pinmux;
} OPPO_SD_POWER;
// Reads ID at PMU address 0x34 and CRC-protected LDO state at 0x64.
// No slave register writes, bus scan, DMA, voltage changes or rail enables.
EFI_STATUS OppoSdPowerInspect (CONST OPPO_I2C_IO *Io, OPPO_SD_POWER *Power);
#endif
