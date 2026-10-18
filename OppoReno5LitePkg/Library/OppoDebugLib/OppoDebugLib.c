/** @file
  Original display-only laboratory DebugLib and hand-drawn 5x7 glyphs.
  Shared cursor state lives in a reserved RAM page, never in a partition.
  SPDX-License-Identifier: BSD-2-Clause
**/
#include <Uefi.h>
#include <OppoNativeProfile.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/OppoDebugLog.h>
#include <Library/OppoFrameBufferLib.h>
#include <Library/OppoBootLogLib.h>

#define LOG_SIGNATURE 0x31474c4fU
#define LOG_REVISION 2U
#define FLOW_SIGNATURE 0x31464c42U
#define RESET_REPORT_TOP 1792U
#define RESET_REPORT_HEIGHT 256U
#define FIRST_ROW 320U
#define CELL_WIDTH 12U
#define CELL_HEIGHT 16U
#define COLUMNS (OPPO_NATIVE_FB_WIDTH / CELL_WIDTH)
#define ROWS ((OPPO_NATIVE_FB_HEIGHT - FIRST_ROW) / CELL_HEIGHT)
#define FRAME_BYTES (OPPO_NATIVE_FB_STRIDE * OPPO_NATIVE_FB_HEIGHT)
#define LOG_LEVELS (DEBUG_INIT | DEBUG_WARN | DEBUG_LOAD | DEBUG_INFO | DEBUG_DISPATCH | DEBUG_ERROR)

typedef struct {
  UINT32 Signature;
  UINT32 Revision;
  UINT32 Busy;
  UINT32 Column;
  UINT32 Row;
  OPPO_FB_CONTEXT Frame;
  UINT32 FlowSignature;
  OPPO_BOOT_FLOW Flow;
} LOG_STATE;
STATIC_ASSERT (sizeof (LOG_STATE) <= 4096, "Log state exceeds its reserved page");
#ifndef OPPO_LOG_BOOT_API_ONLY
STATIC BOOLEAN mReady;
#endif

typedef struct { CHAR8 Character; UINT8 Rows[7]; } GLYPH;
STATIC CONST GLYPH mGlyphs[] = {
  {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
  {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
  {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
  {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
  {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,2,18,12}},
  {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
  {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,25,21,19,19,17}},
  {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
  {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
  {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
  {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
  {'W',{17,17,17,21,21,27,17}}, {'X',{17,17,10,4,10,17,17}},
  {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
  {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
  {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
  {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
  {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
  {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
  {':',{0,4,4,0,4,4,0}}, {'.',{0,0,0,0,0,4,4}},
  {',',{0,0,0,0,4,4,8}}, {'-',{0,0,0,31,0,0,0}},
  {'_',{0,0,0,0,0,0,31}}, {'=',{0,0,31,0,31,0,0}},
  {'/',{1,2,2,4,8,8,16}}, {'\\',{16,8,8,4,2,2,1}},
  {'[',{14,8,8,8,8,8,14}}, {']',{14,2,2,2,2,2,14}},
  {'(',{2,4,8,8,8,4,2}}, {')',{8,4,2,2,2,4,8}},
  {'+',{0,4,4,31,4,4,0}}, {'*',{0,17,10,31,10,17,0}},
  {'?',{14,17,1,2,4,0,4}}, {'!',{4,4,4,4,4,0,4}},
  {'<',{2,4,8,16,8,4,2}}, {'>',{8,4,2,1,2,4,8}},
  {'%',{25,26,2,4,8,11,19}}, {'#',{10,10,31,10,31,10,10}},
  {' ',{0,0,0,0,0,0,0}}
};

STATIC LOG_STATE *State (VOID)
{
  LOG_STATE *S = (LOG_STATE *)(UINTN)OPPO_NATIVE_DEBUG_BASE;
  if (S->Signature != LOG_SIGNATURE || S->Revision != LOG_REVISION || S->Column >= COLUMNS ||
      S->Row >= ROWS || S->Frame.Pixels != (volatile UINT32 *)(UINTN)OPPO_NATIVE_FB_BASE ||
      S->Frame.MappedBytes != FRAME_BYTES || S->Frame.Layout.Width != OPPO_NATIVE_FB_WIDTH ||
      S->Frame.Layout.Height != OPPO_NATIVE_FB_HEIGHT ||
      S->Frame.Layout.StrideBytes != OPPO_NATIVE_FB_STRIDE ||
      S->Frame.Layout.RedShift != OPPO_NATIVE_FB_RED_SHIFT ||
      S->Frame.Layout.BlueShift != OPPO_NATIVE_FB_BLUE_SHIFT ||
      !S->Frame.Layout.OpaqueAlpha) return NULL;
  return S;
}

STATIC VOID Fill (LOG_STATE *S, EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color,
                  UINTN X, UINTN Y, UINTN Width, UINTN Height)
{
  OppoFbBlt (&S->Frame, &Color, sizeof (Color), EfiBltVideoFill,
             0, 0, X, Y, Width, Height, 0);
}

STATIC CONST GLYPH *FindGlyph (CHAR8 C)
{
  if (C >= 'a' && C <= 'z') C = (CHAR8)(C - 'a' + 'A');
  for (UINTN I = 0; I < ARRAY_SIZE (mGlyphs); I++)
    if (mGlyphs[I].Character == C) return &mGlyphs[I];
  for (UINTN I = 0; I < ARRAY_SIZE (mGlyphs); I++)
    if (mGlyphs[I].Character == '?') return &mGlyphs[I];
  return NULL;
}

#ifndef OPPO_LOG_BOOT_API_ONLY
STATIC VOID NewLine (LOG_STATE *S)
{
  S->Column = 0;
  if (++S->Row == ROWS) {
    Fill (S, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0,0,0,0}, 0, FIRST_ROW,
          COLUMNS * CELL_WIDTH, ROWS * CELL_HEIGHT);
    S->Row = 0;
  }
}

STATIC VOID Character (LOG_STATE *S, CHAR8 C, EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color)
{
  if (C == '\r') { S->Column = 0; return; }
  if (C == '\n') { NewLine (S); return; }
  if (C == '\t') {
    for (UINTN I = 0; I < 4; I++) Character (S, ' ', Color);
    return;
  }
  CONST GLYPH *Glyph = FindGlyph (C);
  if (Glyph == NULL) return;
  UINTN X = S->Column * CELL_WIDTH, Y = FIRST_ROW + S->Row * CELL_HEIGHT;
  Fill (S, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0,0,0,0}, X, Y, CELL_WIDTH, CELL_HEIGHT);
  for (UINTN Row = 0; Row < 7; Row++)
    for (UINTN Bit = 0; Bit < 5; Bit++)
      if (Glyph->Rows[Row] & (1U << (4 - Bit))) Fill (S, Color, X + Bit * 2, Y + Row * 2, 2, 2);
  if (++S->Column == COLUMNS) NewLine (S);
}

STATIC VOID Write (LOG_STATE *S, CONST CHAR8 *Buffer, UINTN Bytes, UINTN Level)
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color = (Level & DEBUG_ERROR) ?
    (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0,0,255,0} : (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){255,255,255,0};
  for (UINTN I = 0; I < Bytes && I < 255; I++) Character (S, Buffer[I], Color);
  MemoryFence ();
}
#endif

#ifdef OPPO_LOG_BOOT_API_ONLY
STATIC VOID LargeReport (LOG_STATE *S, CONST CHAR8 *Buffer, UINTN Bytes)
{
  UINTN Column = 0, Line = 0;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL White = {255,255,255,0};
  Fill (S, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0,0,0,0}, 0,
        RESET_REPORT_TOP, OPPO_NATIVE_FB_WIDTH, RESET_REPORT_HEIGHT);
  for (UINTN I = 0; I < Bytes && I < 384 && Line < 8; I++) {
    CHAR8 C = Buffer[I];
    if (C == '\n') {Column = 0; Line++; continue;}
    if (Column >= OPPO_NATIVE_FB_WIDTH / 24) continue;
    CONST GLYPH *Glyph = FindGlyph (C);
    if (Glyph != NULL) for (UINTN Row = 0; Row < 7; Row++)
      for (UINTN Bit = 0; Bit < 5; Bit++) if (Glyph->Rows[Row] & (1U << (4 - Bit)))
        Fill (S, White, Column * 24 + Bit * 4,
              RESET_REPORT_TOP + Line * 32 + Row * 4, 4, 4);
    Column++;
  }
  MemoryFence ();
}

VOID EFIAPI OppoBootLogReset (EFI_RESET_TYPE Type, EFI_STATUS Status)
{
  LOG_STATE *S = State ();
  CHAR8 Buffer[384];
  OPPO_BOOT_FLOW Flow = {0, EFI_NOT_STARTED, EFI_NOT_STARTED, EFI_NOT_STARTED, 0};
  if (S == NULL || S->Busy) return;
  if (S->FlowSignature == FLOW_SIGNATURE) Flow = S->Flow;
  UINTN Bytes = AsciiSPrint (Buffer, sizeof (Buffer),
    "OPPO RESET CAPTURE\nTYPE=%u REQUEST=%a\nEFI STATUS=%016Lx\nSTAGE=%u LOAD=%016Lx\nRETURN=%016Lx\nMAP=%016Lx\nFREE SNAPSHOT=%Lu MIB\n15 SECOND CAPTURE HOLD\n",
    (UINT32)Type, Type == EfiResetShutdown ? "POWER OFF" : "RESET", (UINT64)Status,
    Flow.Stage, (UINT64)Flow.LoadStatus, (UINT64)Flow.StartStatus,
    (UINT64)Flow.MapStatus, Flow.ConventionalBytes / (1024 * 1024));
  S->Busy = 1; MemoryFence ();
  LargeReport (S, Buffer, Bytes);
  S->Busy = 0; MemoryFence ();
}
#else
BOOLEAN EFIAPI OppoDebugStart (VOID)
{
  LOG_STATE *S = (LOG_STATE *)(UINTN)OPPO_NATIVE_DEBUG_BASE;
  OPPO_FB_LAYOUT Layout = {OPPO_NATIVE_FB_WIDTH, OPPO_NATIVE_FB_HEIGHT, OPPO_NATIVE_FB_STRIDE,
    OPPO_NATIVE_FB_RED_SHIFT, OPPO_NATIVE_FB_BLUE_SHIFT, TRUE};
  ZeroMem (S, 4096);
  if (!OppoFbInitialize (&S->Frame, (VOID *)(UINTN)OPPO_NATIVE_FB_BASE, FRAME_BYTES, &Layout)) return FALSE;
  S->Revision = LOG_REVISION;
  Fill (S, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0,0,0,0}, 0, FIRST_ROW,
        COLUMNS * CELL_WIDTH, ROWS * CELL_HEIGHT);
  MemoryFence (); S->Signature = LOG_SIGNATURE; MemoryFence ();
  mReady = TRUE;
  Write (S, "OPPO ORIGINAL SEC/DXE DISPLAY LOG\n", 34, DEBUG_INFO);
  return TRUE;
}

RETURN_STATUS EFIAPI OppoDebugConstructor (VOID)
{
  // SEC explicitly starts the log after validation. The core binds that same
  // reserved page from its constructor; early SEC debug calls stay disabled.
  mReady = (BOOLEAN)(State () != NULL);
  return RETURN_SUCCESS;
}

STATIC LOG_STATE *ReadyState (VOID)
{
#ifdef OPPO_LOG_EARLY_CORE
  // This variant is linked only into the core, after SEC validated and reserved
  // the page. Its early memory assertions precede library constructors.
  if (!mReady) mReady = (BOOLEAN)(State () != NULL);
#endif
  return mReady ? State () : NULL;
}

VOID EFIAPI OppoDebugRecordBootFlow (CONST OPPO_BOOT_FLOW *Flow)
{
  LOG_STATE *S = ReadyState ();
  if (S == NULL || S->Busy || Flow == NULL) return;
  S->Busy = 1; MemoryFence ();
  S->Flow = *Flow; MemoryFence ();
  S->FlowSignature = FLOW_SIGNATURE; MemoryFence ();
  S->Busy = 0; MemoryFence ();
}

VOID EFIAPI DebugVPrint (UINTN ErrorLevel, CONST CHAR8 *Format, VA_LIST Marker)
{
  CHAR8 Buffer[256];
  LOG_STATE *S = ReadyState ();
  if (S == NULL || S->Busy || Format == NULL || !(ErrorLevel & LOG_LEVELS)) return;
  S->Busy = 1; MemoryFence ();
  UINTN Bytes = AsciiVSPrint (Buffer, sizeof (Buffer), Format, Marker);
  Write (S, Buffer, Bytes, ErrorLevel);
  S->Busy = 0; MemoryFence ();
}

VOID EFIAPI DebugPrint (UINTN ErrorLevel, CONST CHAR8 *Format, ...)
{
  VA_LIST Marker;
  VA_START (Marker, Format); DebugVPrint (ErrorLevel, Format, Marker); VA_END (Marker);
}

VOID EFIAPI DebugBPrint (UINTN ErrorLevel, CONST CHAR8 *Format, BASE_LIST Marker)
{
  CHAR8 Buffer[256];
  LOG_STATE *S = ReadyState ();
  if (S == NULL || S->Busy || Format == NULL || !(ErrorLevel & LOG_LEVELS)) return;
  S->Busy = 1; MemoryFence ();
  UINTN Bytes = AsciiBSPrint (Buffer, sizeof (Buffer), Format, Marker);
  Write (S, Buffer, Bytes, ErrorLevel);
  S->Busy = 0; MemoryFence ();
}

VOID EFIAPI DebugAssert (CONST CHAR8 *FileName, UINTN LineNumber, CONST CHAR8 *Description)
{
  STATIC BOOLEAN Asserting;
  if (Asserting) return;
  Asserting = TRUE;
  // Do not put the host's full build path on the physical screen.
  CONST CHAR8 *Base = FileName == NULL ? "UNKNOWN" : FileName;
  if (FileName != NULL)
    for (UINTN I = 0; I < 1024 && FileName[I]; I++)
      if (FileName[I] == '/' || FileName[I] == '\\') Base = FileName + I + 1;
  DebugPrint (DEBUG_ERROR, "\nASSERT %a:%u %a\n", Base, (UINT32)LineNumber,
              Description == NULL ? "UNKNOWN" : Description);
  for (;;) CpuDeadLoop ();
}

VOID *EFIAPI DebugClearMemory (VOID *Buffer, UINTN Length)
{
  if (Buffer == NULL || Length > MAX_UINTN - (UINTN)Buffer) {
    DebugAssert ("OppoDebugLib.c", 0, "INVALID CLEAR RANGE");
    return NULL;
  }
  return SetMem (Buffer, Length, 0xaf);
}
BOOLEAN EFIAPI DebugAssertEnabled (VOID) { return TRUE; }
BOOLEAN EFIAPI DebugPrintEnabled (VOID) { return TRUE; }
BOOLEAN EFIAPI DebugCodeEnabled (VOID) { return TRUE; }
BOOLEAN EFIAPI DebugClearMemoryEnabled (VOID) { return FALSE; }
BOOLEAN EFIAPI DebugPrintLevelEnabled (UINTN Level) { return (BOOLEAN)((Level & LOG_LEVELS) != 0); }
#endif
