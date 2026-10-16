/** @file
  Entry observations passed by the original ARM64 SEC entry to its C routine.
  SPDX-License-Identifier: BSD-2-Clause
**/
#ifndef OPPO_SEC_ENTRY_H
#define OPPO_SEC_ENTRY_H

#include <Base.h>

typedef struct {
  UINT64 Fdt;
  UINT64 InitialX1;
  UINT64 InitialX2;
  UINT64 InitialX3;
  UINT64 ExceptionLevel;
  UINT64 Sctlr;
  UINT64 CounterFrequency;
  UINT64 Mpidr;
} OPPO_SEC_ENTRY_CONTEXT;

/**
  Future platform SEC routine. Entry only establishes a stack and masked vectors;
  no HOBs, MMU, panel, DXE core or Shell are initialized by the assembly alone.
  The context remains in the entry stack. The C implementation must not return.
**/
VOID
EFIAPI
OppoSecMain (IN CONST OPPO_SEC_ENTRY_CONTEXT *Context);

#endif
