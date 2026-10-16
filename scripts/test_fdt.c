/* SPDX-License-Identifier: BSD-2-Clause */
#include <Library/OppoFdtLib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static UINT32 be32(const UINT8 *p)
{
  return (UINT32)p[0] << 24 | (UINT32)p[1] << 16 | (UINT32)p[2] << 8 | p[3];
}

static void put32(UINT8 *p, UINT32 v)
{
  for (UINTN i = 0; i < 4; i++) p[i] = (UINT8)(v >> (24 - 8 * i));
}

static UINT8 *load(const char *path, UINTN *size)
{
  FILE *f = fopen(path, "rb");
  assert(f && fseek(f, 0, SEEK_END) == 0);
  long bytes = ftell(f);
  assert(bytes >= 40 && bytes <= 2 * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0);
  UINT8 *data = malloc((size_t)bytes);
  assert(data && fread(data, 1, (size_t)bytes, f) == (size_t)bytes);
  fclose(f);
  *size = (UINTN)bytes;
  return data;
}

typedef struct { OPPO_FDT_VIEW *View; UINTN Count; BOOLEAN Abort; } VISIT_STATE;

static EFI_STATUS EFIAPI visit(CONST CHAR8 *path, VOID *context)
{
  VISIT_STATE *state = context;
  CONST VOID *value;
  UINT32 bytes;
  EFI_STATUS status = OppoFdtProperty(state->View, path, "reg", &value, &bytes);
  assert(status == EFI_SUCCESS || status == EFI_NOT_FOUND);
  state->Count++;
  return state->Abort && state->Count == 2 ? EFI_ABORTED : EFI_SUCCESS;
}

static void synthetic_checks(UINT8 *data, UINTN size)
{
  OPPO_FDT_VIEW view, changed;
  OPPO_MEMORY_RANGE range;
  CONST VOID *value, *compatible;
  UINT32 bytes, compatible_bytes;
  UINT8 *mutation = malloc(size + 32);
  assert(mutation && OppoFdtOpen(data, size, &view));
  VISIT_STATE visitor = {&view, 0, FALSE};
  UINT32 count;
  assert(OppoFdtPropertyCount(&view, "/", &count) == EFI_SUCCESS && count == 5);
  assert(OppoFdtPropertyCount(&view, "/chosen", &count) == EFI_SUCCESS && count == 1);
  assert(OppoFdtPropertyCount(&view, "/absent", &count) == EFI_NOT_FOUND && count == 0);
  assert(OppoFdtVisitNodes(&view, visit, &visitor) == EFI_SUCCESS && visitor.Count == 3);
  visitor.Count = 0; visitor.Abort = TRUE;
  assert(OppoFdtVisitNodes(&view, visit, &visitor) == EFI_ABORTED && visitor.Count == 2);
  assert(OppoFdtProperty(&view, "/", "model", &value, &bytes) == EFI_SUCCESS);
  assert(bytes == sizeof("Example board") && memcmp(value, "Example board", bytes) == 0);
  UINTN model_property = (CONST UINT8 *)value - data - 12;
  assert(OppoFdtProperty(&view, "/", "compatible", &compatible, &compatible_bytes) == EFI_SUCCESS);
  UINTN compatible_property = (CONST UINT8 *)compatible - data - 12;
  assert(OppoFdtProperty(&view, "/memory@40000000", "reg", &value, &bytes) == EFI_SUCCESS);
  assert(bytes == 16 && be32((CONST UINT8 *)value + 4) == 0x40000000);
  assert(OppoFdtProperty(&view, "/absent", "reg", &value, &bytes) == EFI_NOT_FOUND);
  assert(value == NULL && bytes == 0 && !OppoFdtReservation(&view, 0, &range));
  for (UINTN length = 0; length < size; length++) assert(!OppoFdtOpen(data, length, &changed));

  // Duplicate a selected property by changing another property's name offset.
  memcpy(mutation, data, size);
  put32(mutation + compatible_property + 8, be32(data + model_property + 8));
  assert(OppoFdtOpen(mutation, size, &changed));
  assert(OppoFdtProperty(&changed, "/", "model", &value, &bytes) == EFI_COMPROMISED_DATA);
  assert(value == NULL && bytes == 0);

  // Insert room for two reservations plus a terminator, moving later blocks.
  UINT32 structure = be32(data + 8), strings = be32(data + 12);
  assert(structure == 56 && be32(data + 16) == 40);
  memcpy(mutation, data, 56);
  memset(mutation + 56, 0, 32);
  memcpy(mutation + 88, data + 56, size - 56);
  put32(mutation + 4, (UINT32)size + 32);
  put32(mutation + 8, structure + 32);
  put32(mutation + 12, strings + 32);
  put32(mutation + 44, 0x48000000);
  put32(mutation + 52, 0x1000);
  assert(OppoFdtOpen(mutation, size + 32, &changed));
  assert(changed.ReserveCount == 1 && OppoFdtReservation(&changed, 0, &range));
  assert(range.Base == 0x48000000 && range.Length == 0x1000);
  put32(mutation + 60, 0x48000800);
  put32(mutation + 68, 0x1000);
  assert(!OppoFdtOpen(mutation, size + 32, &changed));

  // Deterministic corruptions exercise the complete input and token bounds.
  UINT32 random = 0x817252a1;
  for (UINTN test = 0; test < 4096; test++) {
    memcpy(mutation, data, size);
    random ^= random << 13; random ^= random >> 17; random ^= random << 5;
    mutation[random % size] ^= (UINT8)(1U << (test % 8));
    if (OppoFdtOpen(mutation, size, &changed)) {
      EFI_STATUS status = OppoFdtProperty(&changed, "/", "model", &value, &bytes);
      assert(status == EFI_SUCCESS || status == EFI_NOT_FOUND || status == EFI_COMPROMISED_DATA);
      if (status != EFI_SUCCESS) assert(value == NULL && bytes == 0);
    }
  }
  free(mutation);
}

static void private_capture_checks(const char *path)
{
  UINTN size;
  UINT8 *data = load(path, &size);
  OPPO_FDT_VIEW view;
  CONST VOID *value;
  UINT32 bytes;
  assert(OppoFdtOpen(data, size, &view));
  assert(OppoFdtProperty(&view, "/", "compatible", &value, &bytes) == EFI_SUCCESS);
  assert(bytes == sizeof("mediatek,MT6779") && memcmp(value, "mediatek,MT6779", bytes) == 0);
  assert(OppoFdtProperty(&view, "/memory", "reg", &value, &bytes) == EFI_SUCCESS);
  assert(bytes == 16 && be32(value) == 0 && be32((CONST UINT8 *)value + 4) == 0x40000000);
  assert(be32((CONST UINT8 *)value + 8) == 1 && be32((CONST UINT8 *)value + 12) == 0x80000000);
  assert(OppoFdtProperty(&view, "/psci", "method", &value, &bytes) == EFI_SUCCESS);
  assert(bytes == 4 && memcmp(value, "smc", 4) == 0);
  puts("C reader independently confirmed MT6779, 6 GiB RAM and PSCI SMC in the private capture.");
  free(data);
}

int main(int argc, char **argv)
{
  UINTN size;
  assert(argc == 2 || argc == 3);
  UINT8 *fixture = load(argv[1], &size);
  synthetic_checks(fixture, size);
  free(fixture);
  if (argc == 3) private_capture_checks(argv[2]);
  puts("Original FDT reader: truncations, property ambiguity, reservations and 4096 corruptions passed with host sanitizers.");
  return 0;
}
