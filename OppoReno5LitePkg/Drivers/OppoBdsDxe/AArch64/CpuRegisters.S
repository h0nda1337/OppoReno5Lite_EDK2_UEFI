/* SPDX-License-Identifier: BSD-2-Clause */
/* Original local-register read helpers. No MSR, SMC, memory access or CPU start. */
.macro CPU_READ Name, Register
.section .text.\Name, "ax", %progbits
.balign 4
.global \Name
.type \Name, %function
\Name:
  mrs x0, \Register
  ret
.endm

CPU_READ OppoCpuCurrentEl, CurrentEL
CPU_READ OppoCpuMpidrValue, MPIDR_EL1
CPU_READ OppoCpuMidrValue, MIDR_EL1
CPU_READ OppoCpuPfr0Value, ID_AA64PFR0_EL1
CPU_READ OppoCpuSreEl1, S3_0_C12_C12_5
CPU_READ OppoCpuSreEl2, S3_4_C12_C9_5
.section .note.GNU-stack, "", %progbits
