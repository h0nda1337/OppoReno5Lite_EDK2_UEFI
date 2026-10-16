/* SPDX-License-Identifier: BSD-2-Clause */
/* QEMU-only semihosting harness. Never part of an OPPO image. */
#include <OppoSecEntry.h>
#include <stddef.h>

extern CONST UINT8 OppoSecVectors[];
_Static_assert(sizeof(OPPO_SEC_ENTRY_CONTEXT) == 64, "Entry context ABI size");
_Static_assert(offsetof(OPPO_SEC_ENTRY_CONTEXT, ExceptionLevel) == 32, "EL ABI offset");
_Static_assert(offsetof(OPPO_SEC_ENTRY_CONTEXT, Mpidr) == 56, "MPIDR ABI offset");

static void semihost(UINT64 operation, CONST VOID *argument)
{
  register UINT64 x0 __asm__("x0") = operation;
  register CONST VOID *x1 __asm__("x1") = argument;
  __asm__ volatile("hlt #0xf000" : "+r"(x0) : "r"(x1) : "memory");
}

VOID EFIAPI OppoSecMain(IN CONST OPPO_SEC_ENTRY_CONTEXT *context)
{
  UINT64 sp, vectors, daif;
  UINT64 status[2] = {0x20026, 1};
  CONST UINT8 *fdt = (CONST VOID *)(UINTN)context->Fdt;
  BOOLEAN passed;
  __asm__ volatile("mov %0, sp" : "=r"(sp));
  __asm__ volatile("mrs %0, daif" : "=r"(daif));
#if TEST_EXPECTED_EL == 1
  __asm__ volatile("mrs %0, vbar_el1" : "=r"(vectors));
#else
  __asm__ volatile("mrs %0, vbar_el2" : "=r"(vectors));
#endif
  passed = context->ExceptionLevel == TEST_EXPECTED_EL &&
           (sp & 15) == 0 && ((UINTN)context & 15) == 0 && (context->Sctlr & 5) == 0 &&
           (daif & 0x3c0) == 0x3c0 && vectors == (UINTN)OppoSecVectors &&
           context->InitialX1 == 0x1111 && context->InitialX2 == 0x2222 &&
           context->InitialX3 == 0x3333 && context->CounterFrequency != 0 &&
           context->Fdt != 0 && (context->Fdt & 7) == 0 &&
           fdt[0] == 0xd0 && fdt[1] == 0x0d && fdt[2] == 0xfe && fdt[3] == 0xed;
  if (passed) {
    semihost(4, "PASS original SEC entry: handoff, EL, stack, vectors and masked interrupts\n");
    status[1] = 0;
  } else {
    semihost(4, "FAIL SEC entry contract\n");
  }
  semihost(0x20, status);
  for (;;) __asm__ volatile("wfe");
}
