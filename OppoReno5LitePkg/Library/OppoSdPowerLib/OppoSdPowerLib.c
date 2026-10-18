/* SPDX-License-Identifier: BSD-2-Clause */
/* Original bounded register-query engine. Research contracts are cited in
 * docs/MICROSD.md; no manufacturer driver implementation is imported.
 */
#include <Library/OppoSdPowerLib.h>

STATIC UINT32 Read (CONST OPPO_I2C_IO *Io, UINT32 Offset)
{ return Io->Read (Io->Context, Offset); }
STATIC VOID Write (CONST OPPO_I2C_IO *Io, UINT32 Offset, UINT32 Value)
{ Io->Write (Io->Context, Offset, Value); }
STATIC UINT8 Crc (CONST UINT8 *Bytes, UINTN Count)
{
  UINT8 Value = 0;
  for (UINTN I = 0; I < Count; I++) {
    Value ^= Bytes[I];
    for (UINTN Bit = 0; Bit < 8; Bit++)
      Value = (UINT8)((Value << 1) ^ ((Value & 0x80) ? 7 : 0));
  }
  return Value;
}

STATIC EFI_STATUS Query (CONST OPPO_I2C_IO *Io, OPPO_SD_POWER *P,
                         UINT8 Address, UINT8 Register, BOOLEAN Protected, UINT8 *Value)
{
  UINT8 Data, Check;
  P->LastAddress = Address; P->LastRegister = Register; P->Interrupt = 0;
  if (Read (Io, 0x24) & 1) return EFI_NOT_READY;
  // One command byte, repeated start, one data byte (+CRC for the LDO bank).
  Write (Io, 0x08, 0); // Polling; no controller interrupt delivery.
  Write (Io, 0x0c, 0x1ff); Write (Io, 0x38, 1);
  Write (Io, 0x10, 0x3a); // ACK detection, clock extension, direction change, RS.
  Write (Io, 0x04, Address << 1); Write (Io, 0x14, 1);
  Write (Io, 0x44, Protected ? 2 : 1); Write (Io, 0x18, 2);
  Write (Io, 0x00, Register); // Index for a read, never a register-value payload.
  Write (Io, 0x24, 1);
  for (UINT32 Elapsed = 0; Elapsed < 100000; Elapsed += 10) {
    UINT32 Events = Read (Io, 0x0c) & 0x1ff;
    P->Interrupt = Events;
    if (Events & ~1U) return (Events & (1U << 5)) ? EFI_TIMEOUT : EFI_DEVICE_ERROR;
    if (Events & 1) {
      Write (Io, 0x0c, Events);
      Data = (UINT8)Read (Io, 0x00);
      if (Protected) {
        Check = (UINT8)Read (Io, 0x00);
        UINT8 Packet[3] = {(UINT8)((Address << 1) | 1), Register, Data};
        if (Crc (Packet, sizeof (Packet)) != Check) return EFI_CRC_ERROR;
      }
      *Value = Data;
      return EFI_SUCCESS;
    }
    Io->Delay (Io->Context, 10);
  }
  return EFI_TIMEOUT;
}

EFI_STATUS OppoSdPowerInspect (CONST OPPO_I2C_IO *Io, OPPO_SD_POWER *P)
{
  STATIC CONST UINT32 Registers[] = {0x04,0x08,0x10,0x14,0x18,0x1c,0x20,0x28,0x2c,0x30,0x34,0x44,0x48,0x4c};
  UINT32 Saved[sizeof (Registers) / sizeof (Registers[0])];
  STATIC CONST UINT8 LdoRegisters[] = {4,5,9,10,11,15};
  UINT8 *Destinations[6];
  if (P == NULL) return EFI_INVALID_PARAMETER;
  P->Status = P->Restore = EFI_NOT_STARTED; P->Valid = 0;
  P->Interrupt = 0; P->LastAddress = P->LastRegister = MAX_UINT32;
  P->RestoreRegister = MAX_UINT32; P->RestoreExpected = P->RestoreObserved = 0;
  P->ChipId = P->VmcControl1 = P->VmcControl2 = P->VmcVoltage = 0;
  P->VmchControl1 = P->VmchControl2 = P->VmchVoltage = 0;
  if (Io == NULL || Io->Read == NULL || Io->Write == NULL || Io->Delay == NULL)
    return P->Status = EFI_INVALID_PARAMETER;
  if (Read (Io, 0x24) & 1) return P->Status = EFI_NOT_READY;
  for (UINTN I = 0; I < sizeof (Registers) / sizeof (Registers[0]); I++) Saved[I] = Read (Io, Registers[I]);
  // Retain the shared source selection. Maximum legacy divisors keep the bus
  // slow for all three MT6779 i2c_sel sources; divisor 5 matches this board FDT.
  Write (Io, 0x08, 0); Write (Io, 0x48, 0x0404);
  Write (Io, 0x20, 0x073f); Write (Io, 0x2c, 0x01ff);
  Write (Io, 0x30, 0); Write (Io, 0x34, 0); // Legacy rate, board push-pull binding.
  Write (Io, 0x28, 0x8001); Write (Io, 0x1c, 10); Write (Io, 0x4c, 0xffff);
  P->Status = Query (Io, P, 0x34, 0, FALSE, &P->ChipId);
  if (!EFI_ERROR (P->Status)) {
    P->Valid = 1;
    if ((P->ChipId & 0xf0) != 0x50) P->Status = EFI_UNSUPPORTED;
  }
  Destinations[0] = &P->VmcControl1; Destinations[1] = &P->VmcControl2;
  Destinations[2] = &P->VmcVoltage; Destinations[3] = &P->VmchControl1;
  Destinations[4] = &P->VmchControl2; Destinations[5] = &P->VmchVoltage;
  for (UINTN I = 0; I < 6 && !EFI_ERROR (P->Status); I++) {
    P->Status = Query (Io, P, 0x64, LdoRegisters[I], TRUE, Destinations[I]);
    if (!EFI_ERROR (P->Status)) P->Valid |= 1U << (I + 1);
  }
  // Abort a failed transaction before restoring the retained configuration.
  if (Read (Io, 0x24) & 1) {
    Write (Io, 0x50, 1); Io->Delay (Io->Context, 10);
  }
  Write (Io, 0x0c, 0x1ff); Write (Io, 0x38, 1);
  UINT32 Start = Read (Io, 0x24);
  P->Restore = (Start & 1) ? EFI_DEVICE_ERROR : EFI_SUCCESS;
  if (EFI_ERROR (P->Restore)) {
    P->RestoreRegister = 0x24; P->RestoreExpected = Start & ~1U; P->RestoreObserved = Start;
  }
  if (!EFI_ERROR (P->Restore)) {
    // Interrupt mask last; no pending interrupt is restored.
    for (UINTN I = 0; I < sizeof (Registers) / sizeof (Registers[0]); I++)
      if (Registers[I] != 0x08) Write (Io, Registers[I], Saved[I]);
    Write (Io, 0x08, Saved[1]);
    for (UINTN I = 0; I < sizeof (Registers) / sizeof (Registers[0]); I++) {
      UINT32 Observed = Read (Io, Registers[I]);
      if (Observed != Saved[I]) {
        P->Restore = EFI_DEVICE_ERROR;
        if (P->RestoreRegister == MAX_UINT32) {
          P->RestoreRegister = Registers[I]; P->RestoreExpected = Saved[I]; P->RestoreObserved = Observed;
        }
      }
    }
  }
  return P->Status;
}
