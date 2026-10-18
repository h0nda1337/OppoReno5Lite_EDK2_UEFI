/* SPDX-License-Identifier: BSD-2-Clause */
/* Original PIO implementation from the MSDC register contract and SD protocol.
 * Only this controller's register offsets are accepted by the injected backend.
 * The board must supply a verified 26 MHz source, pads and 3 V SD rails.
 * No DMA, tuning, voltage switch, card erase or storage write is implemented.
 */
#include <Library/OppoMsdcLib.h>

#define CFG       0x00
#define IOCON     0x04
#define PS        0x08
#define INT       0x0c
#define INTEN     0x10
#define FIFO      0x14
#define RXDATA    0x1c
#define SDC_CFG   0x30
#define CMD       0x34
#define ARG       0x38
#define STS       0x3c
#define RESP0     0x40
#define BLK_NUM   0x50
#define CFG_RST   (1U << 2)
#define CFG_STABLE (1U << 7)
#define CMD_READY (1U << 8)
#define CMD_TIMEOUT (1U << 9)
#define CMD_CRC   (1U << 10)
#define DATA_DONE (1U << 12)
#define DATA_TIMEOUT (1U << 14)
#define DATA_CRC  (1U << 15)
#define CMD_EVENTS (CMD_READY | CMD_TIMEOUT | CMD_CRC)
#define DATA_EVENTS (DATA_DONE | DATA_TIMEOUT | DATA_CRC)
#define FIFO_CLEAR (1U << 31)
#define R1_ERRORS 0xfff98008U

STATIC UINT32 Rd (OPPO_MSDC *C, UINT32 Offset)
{ return C->Io.Read32 (C->Io.Context, Offset); }
STATIC VOID Wr (OPPO_MSDC *C, UINT32 Offset, UINT32 Value)
{ C->Io.Write32 (C->Io.Context, Offset, Value); }
STATIC VOID Delay (OPPO_MSDC *C, UINT32 Us)
{ C->Io.DelayUs (C->Io.Context, Us); }

STATIC EFI_STATUS WaitBits (OPPO_MSDC *C, UINT32 Offset, UINT32 Mask,
                           UINT32 Expected, UINT32 LimitUs)
{
  for (UINT32 Time = 0; Time < LimitUs; Time += 10) {
    if ((Rd (C, Offset) & Mask) == Expected) return EFI_SUCCESS;
    Delay (C, 10);
  }
  return EFI_TIMEOUT;
}

STATIC EFI_STATUS EmptyFifo (OPPO_MSDC *C)
{
  Wr (C, FIFO, FIFO_CLEAR);
  return WaitBits (C, FIFO, FIFO_CLEAR, 0, 10000);
}

STATIC EFI_STATUS Clock (OPPO_MSDC *C, UINT32 Divisor)
{
  // Legacy SDR: zero divides input by2; nonzero divides by4*divisor.
  // The board source remains26MHz: 17 ->382352Hz, 1 ->6.5MHz, 0 ->13MHz.
  UINT32 Value = Rd (C, CFG);
  Value &= ~((0x7fffU << 8) | CFG_RST);
  Value |= (Divisor << 8) | 0x0bU; // SD mode, continuous clock, PIO.
  Wr (C, CFG, Value);
  EFI_STATUS Status = WaitBits (C, CFG, CFG_STABLE, CFG_STABLE, 100000);
  if (!EFI_ERROR (Status) && (Rd (C, CFG) & (0x7fffU << 8)) != Divisor << 8)
    Status = EFI_DEVICE_ERROR;
  C->ClockHz = EFI_ERROR (Status) ? 0 : Divisor == 0 ? 13000000 : 26000000 / (4 * Divisor);
  return Status;
}

STATIC EFI_STATUS SendCommand (OPPO_MSDC *C, UINT32 Opcode, UINT32 Argument,
                              UINT32 ResponseType, UINT32 DataBytes, UINT32 Response[4])
{
  UINT32 Busy = Opcode == 0 ? 2 : 3;
  EFI_STATUS Status = WaitBits (C, STS, Busy, 0, 100000);
  if (EFI_ERROR (Status)) return Status;
  C->LastCommand = Opcode;
  Wr (C, INT, CMD_EVENTS);
  Wr (C, ARG, Argument);
  Wr (C, CMD, Opcode | (ResponseType << 7) | (DataBytes ? (1U << 11) | (DataBytes << 16) : 0));
  for (UINT32 Time = 0; Time < 100000; Time += 10) {
    UINT32 Events = Rd (C, INT) & CMD_EVENTS;
    C->LastInterrupt = Events;
    if (Events) {
      Wr (C, INT, Events);
      if (Events & CMD_TIMEOUT) return EFI_TIMEOUT;
      if (Events & CMD_CRC) return EFI_CRC_ERROR;
      if (ResponseType == 2) {
        for (UINT32 I = 0; I < 4; I++) Response[I] = Rd (C, RESP0 + (3 - I) * 4);
      } else if (ResponseType != 0) Response[0] = Rd (C, RESP0);
      if (ResponseType == 7)
        return WaitBits (C, PS, 1U << 16, 1U << 16, 500000);
      return EFI_SUCCESS;
    }
    Delay (C, 10);
  }
  return EFI_TIMEOUT;
}

STATIC EFI_STATUS Command (OPPO_MSDC *C, UINT32 Opcode, UINT32 Argument,
                          UINT32 ResponseType, BOOLEAN Data, UINT32 Response[4])
{ return SendCommand (C, Opcode, Argument, ResponseType, Data ? 512U : 0U, Response); }

STATIC EFI_STATUS ReadData (OPPO_MSDC *C, UINT8 *Destination, UINT32 Bytes)
{
  UINT32 Copied = 0;
  BOOLEAN Done = FALSE;
  for (UINT32 Time = 0; Time < 1000000; Time += 10) {
    UINT32 Events = Rd (C, INT) & DATA_EVENTS;
    C->LastInterrupt = Events;
    if (Events & (DATA_TIMEOUT | DATA_CRC)) {
      Wr (C, INT, Events);
      return Events & DATA_CRC ? EFI_CRC_ERROR : EFI_TIMEOUT;
    }
    if (Events & DATA_DONE) Done = TRUE;
    UINT32 Available = Rd (C, FIFO) & 0xff;
    while (Available >= 4 && Copied < Bytes) {
      UINT32 Value = Rd (C, RXDATA);
      for (UINT32 I = 0; I < 4; I++) Destination[Copied + I] = (UINT8)(Value >> (I * 8));
      Copied += 4; Available -= 4;
    }
    if (Copied == Bytes && Done) {
      // Include errors reported with completion or the last FIFO drain.
      Events = Rd (C, INT) & DATA_EVENTS; C->LastInterrupt = Events;
      Wr (C, INT, DATA_EVENTS);
      if (Events & DATA_CRC) return EFI_CRC_ERROR;
      if (Events & DATA_TIMEOUT) return EFI_TIMEOUT;
      return EFI_SUCCESS;
    }
    Delay (C, 10);
  }
  return EFI_TIMEOUT;
}

STATIC EFI_STATUS R1 (OPPO_MSDC *C, UINT32 Opcode, UINT32 Argument, UINT32 Type)
{
  UINT32 Response[4];
  EFI_STATUS Status = Command (C, Opcode, Argument, Type, FALSE, Response);
  if (!EFI_ERROR (Status) && (Response[0] & R1_ERRORS)) return EFI_DEVICE_ERROR;
  return Status;
}

EFI_STATUS OppoMsdcInitialize (OPPO_MSDC *C, CONST OPPO_MSDC_IO *Io)
{
  UINT32 Response[4], Csd[4];
  EFI_STATUS Status;
  BOOLEAN Version2;
  if (C == NULL || Io == NULL || Io->Read32 == NULL || Io->Write32 == NULL || Io->DelayUs == NULL)
    return EFI_INVALID_PARAMETER;
  C->Io = *Io; C->Ready = FALSE; C->Blocks = 0; C->Rca = 0;
  C->LastCommand = MAX_UINT32; C->LastInterrupt = 0; C->BlockAddressed = FALSE;
  C->InitialIocon = C->Cmd8Attempts = 0; C->Cmd8FirstStatus = EFI_NOT_STARTED;
  C->BusWidth = 1; C->ClockHz = C->MaximumClockHz = 0; C->FastSamplesVerified = FALSE;
  Wr (C, INTEN, 0);
  Wr (C, CFG, Rd (C, CFG) | CFG_RST);
  Status = WaitBits (C, CFG, CFG_RST, 0, 100000);
  if (EFI_ERROR (Status)) return Status;
  Status = Clock (C, 17);
  if (EFI_ERROR (Status)) return Status;
  Status = EmptyFifo (C);
  if (EFI_ERROR (Status)) return Status;
  Wr (C, INT, 0xffffffffU);
  Wr (C, SDC_CFG, (Rd (C, SDC_CFG) & ~((3U << 16) | (1U << 20))) | (1U << 19) | (0xffU << 24));
  Wr (C, PS, Rd (C, PS) & ~1U); // Do not enable the controller's card-detect IRQ.
  C->InitialIocon = Rd (C, IOCON);
  // Legacy one-bit SDR uses a common rising data edge, no per-line sampling,
  // no DDR50/SDR104 clock selector and no separate data-delay selection.
  // Preserve unrelated and write-side fields. Controller reset does not promise
  // to discard all timing state left by an earlier software stage.
  UINT32 LegacyIocon = C->InitialIocon & ~0x00ff003fU;
  Wr (C, IOCON, LegacyIocon);
  Delay (C, 2000); // More than 74 initialization clocks at the selected rate.
  Status = Command (C, 0, 0, 0, FALSE, Response);
  if (EFI_ERROR (Status)) return Status;
  Status = Command (C, 8, 0x1aa, 1, FALSE, Response);
  C->Cmd8FirstStatus = Status; C->Cmd8Attempts = 1;
  if (Status == EFI_CRC_ERROR) {
    // A single bounded alternate response edge. Never accept a CRC-failed
    // response and never apply this fallback to media reads or their errors.
    Wr (C, IOCON, LegacyIocon | (1U << 1));
    Status = Command (C, 8, 0x1aa, 1, FALSE, Response);
    C->Cmd8Attempts = 2;
    // A CRC on either attempt is not evidence for the SD-v1 timeout fallback.
    if (Status != EFI_SUCCESS) return Status;
  }
  Version2 = Status == EFI_SUCCESS;
  if (Version2 && (Response[0] & 0xfff) != 0x1aa) return EFI_UNSUPPORTED;
  if (Status != EFI_SUCCESS && Status != EFI_TIMEOUT) return Status;
  for (UINT32 Attempt = 0; Attempt < 200; Attempt++) {
    Status = R1 (C, 55, 0, 1);
    if (EFI_ERROR (Status)) return Status;
    Status = Command (C, 41, 0x00ff8000U | (Version2 ? 1U << 30 : 0), 3, FALSE, Response);
    if (EFI_ERROR (Status)) return Status;
    if ((Response[0] & 0x00ff8000U) == 0) return EFI_UNSUPPORTED;
    if (Response[0] & (1U << 31)) break;
    if (Attempt == 199) return EFI_TIMEOUT;
    Delay (C, 10000);
  }
  C->BlockAddressed = Version2 && (Response[0] & (1U << 30)) != 0;
  Status = Command (C, 2, 0, 2, FALSE, Response); // CID is neither stored nor logged.
  if (EFI_ERROR (Status)) return Status;
  Status = Command (C, 3, 0, 1, FALSE, Response);
  if (EFI_ERROR (Status)) return Status;
  if ((Response[0] & 0xe000) || (Response[0] >> 16) == 0) return EFI_DEVICE_ERROR;
  C->Rca = Response[0] & 0xffff0000U;
  Status = Command (C, 9, C->Rca, 2, FALSE, Csd);
  if (EFI_ERROR (Status)) return Status;
  // TRAN_SPEED, CSD bits103:96. Reserved encodings never justify acceleration.
  STATIC CONST UINT32 Units[] = {100000, 1000000, 10000000, 100000000};
  STATIC CONST UINT8 Multipliers[] = {0,10,12,13,15,20,25,30,35,40,45,50,55,60,70,80};
  UINT32 Speed = Csd[0] & 255;
  if (!(Speed & 128) && (Speed & 7) < ARRAY_SIZE (Units))
    C->MaximumClockHz = (UINT32)((UINT64)Units[Speed & 7] * Multipliers[(Speed >> 3) & 15] / 10);
  if ((Csd[0] >> 30) == 1 && C->BlockAddressed) {
    UINT32 Size = ((Csd[1] & 0x3f) << 16) | (Csd[2] >> 16);
    C->Blocks = ((UINT64)Size + 1) * 1024;
  } else if ((Csd[0] >> 30) == 0 && !C->BlockAddressed) {
    UINT32 ReadBits = (Csd[1] >> 16) & 15;
    UINT32 Size = ((Csd[1] & 0x3ff) << 2) | (Csd[2] >> 30);
    UINT32 Mult = (Csd[2] >> 15) & 7;
    if (ReadBits < 9 || ReadBits > 11) return EFI_UNSUPPORTED;
    C->Blocks = ((UINT64)Size + 1) << (Mult + 2 + ReadBits - 9);
  } else return EFI_UNSUPPORTED;
  Status = R1 (C, 7, C->Rca, 7);
  if (EFI_ERROR (Status)) return Status;
  if (!C->BlockAddressed) {
    Status = R1 (C, 16, 512, 1);
    if (EFI_ERROR (Status)) return Status;
  }
  Status = Clock (C, 1);
  if (EFI_ERROR (Status)) return Status;
  C->Ready = TRUE;
  return EFI_SUCCESS;
}

EFI_STATUS OppoMsdcRead (OPPO_MSDC *C, UINT64 Lba, UINTN Blocks, VOID *Buffer)
{
  UINT8 *Destination = Buffer;
  UINT32 Response[4];
  EFI_STATUS Status;
  if (C == NULL || !C->Ready) return EFI_NOT_READY;
  if (Blocks == 0) return EFI_SUCCESS;
  if (Buffer == NULL || Blocks > MAX_UINTN / 512 || Lba >= C->Blocks || Blocks > C->Blocks - Lba)
    return EFI_INVALID_PARAMETER;
  if (!C->BlockAddressed && Lba + Blocks - 1 > MAX_UINT32 / 512) return EFI_INVALID_PARAMETER;
  for (UINTN Block = 0; Block < Blocks; Block++) {
    UINT64 Sector = Lba + Block;
    if (C->BlockAddressed && Sector > MAX_UINT32) return EFI_INVALID_PARAMETER;
    Status = EmptyFifo (C);
    if (EFI_ERROR (Status)) goto Failed;
    Wr (C, INT, DATA_EVENTS);
    Wr (C, BLK_NUM, 1);
    Status = Command (C, 17, (UINT32)(C->BlockAddressed ? Sector : Sector * 512), 1, TRUE, Response);
    if (EFI_ERROR (Status)) goto Failed;
    if (Response[0] & R1_ERRORS) { Status = EFI_DEVICE_ERROR; goto Failed; }
    Status = ReadData (C, Destination, 512);
    if (EFI_ERROR (Status)) goto Failed;
    Destination += 512;
  }
  return EFI_SUCCESS;
Failed:
  // Require explicit reinitialization after an uncertain transfer/card state.
  C->Ready = FALSE;
  return Status;
}

STATIC EFI_STATUS AppPrefix (OPPO_MSDC *C)
{
  UINT32 Response[4];
  EFI_STATUS Status = Command (C, 55, C->Rca, 1, FALSE, Response);
  if (!EFI_ERROR (Status) && ((Response[0] & R1_ERRORS) || !(Response[0] & (1U << 5))))
    return EFI_DEVICE_ERROR;
  return Status;
}

EFI_STATUS OppoMsdcEnableFastRead (OPPO_MSDC *C)
{
  UINT8 Baseline[4][512], Compared[512], Scr[8];
  UINT32 Response[4];
  EFI_STATUS Status;
  if (C == NULL || !C->Ready) return EFI_NOT_READY;
  if (C->FastSamplesVerified) return EFI_ALREADY_STARTED;
  if (C->BusWidth != 1 || C->ClockHz != 6500000 || C->MaximumClockHz < 13000000 || C->Blocks < 4)
    return EFI_UNSUPPORTED;
  UINT64 Sectors[] = {0, 1, C->Blocks / 2, C->Blocks - 1};
  for (UINTN I = 0; I < ARRAY_SIZE (Sectors); I++) {
    Status = OppoMsdcRead (C, Sectors[I], 1, Baseline[I]);
    if (EFI_ERROR (Status)) goto Failed;
  }
  Status = AppPrefix (C);
  if (EFI_ERROR (Status)) goto Failed;
  Status = EmptyFifo (C);
  if (EFI_ERROR (Status)) goto Failed;
  Wr (C, INT, DATA_EVENTS); Wr (C, BLK_NUM, 1);
  Status = SendCommand (C, 51, 0, 1, 8, Response);
  if (EFI_ERROR (Status)) goto Failed;
  if (Response[0] & R1_ERRORS) {Status = EFI_DEVICE_ERROR; goto Failed;}
  Status = ReadData (C, Scr, sizeof (Scr));
  if (EFI_ERROR (Status)) goto Failed;
  if ((Scr[0] >> 4) != 0 || (Scr[1] & 5) != 5) {Status = EFI_UNSUPPORTED; goto Failed;}
  Status = AppPrefix (C);
  if (EFI_ERROR (Status)) goto Failed;
  Status = R1 (C, 6, 2, 1); // ACMD6: volatile bus-width selection, not a storage write.
  if (EFI_ERROR (Status)) goto Failed;
  UINT32 Config = (Rd (C, SDC_CFG) & ~(3U << 16)) | (1U << 16);
  Wr (C, SDC_CFG, Config);
  if (Rd (C, SDC_CFG) != Config) {Status = EFI_DEVICE_ERROR; goto Failed;}
  C->BusWidth = 4;
  Status = Clock (C, 0);
  if (EFI_ERROR (Status)) goto Failed;
  for (UINTN I = 0; I < ARRAY_SIZE (Sectors); I++) {
    Status = OppoMsdcRead (C, Sectors[I], 1, Compared);
    if (EFI_ERROR (Status)) goto Failed;
    for (UINTN B = 0; B < sizeof (Compared); B++)
      if (Compared[B] != Baseline[I][B]) {Status = EFI_COMPROMISED_DATA; goto Failed;}
  }
  C->FastSamplesVerified = TRUE;
  return EFI_SUCCESS;
Failed:
  C->Ready = FALSE;
  return Status;
}
