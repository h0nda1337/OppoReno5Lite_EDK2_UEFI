/* SPDX-License-Identifier: BSD-2-Clause */
/* Original native SEC entry. No loader, relocation or appended payload stage.
 * Linux-style ARM64 handoff: physical FDT in x0, MMU/data cache already off.
 * Standalone QEMU fixtures test this entry; it is not yet a complete firmware.
 */
.section .text.OppoSecEntry, "ax", %progbits
.balign 16
.global OppoSecEntry
.global _ModuleEntryPoint
.set _ModuleEntryPoint, OppoSecEntry
.type OppoSecEntry, %function
OppoSecEntry:
    msr     daifset, #0xf
    // Preserve every handoff argument before using scratch registers.
    mov     x20, x0
    mov     x21, x1
    mov     x22, x2
    mov     x23, x3
    mrs     x24, CurrentEL
    mrs     x27, mpidr_el1
    and     x9, x27, #0xffffff
    cbnz    x9, OppoSecPark
    ubfx    x9, x27, #32, #8
    cbnz    x9, OppoSecPark
    cmp     x24, #4
    b.eq    .LEl1
    cmp     x24, #8
    b.ne    OppoSecPark
    mrs     x25, sctlr_el2
    b       .LCheckState
.LEl1:
    mrs     x25, sctlr_el1
.LCheckState:
    // Reject an unsupported address/cache state; do not disable it blindly.
    tst     x25, #1
    b.ne    OppoSecPark
    tst     x25, #4
    b.ne    OppoSecPark
    adrp    x9, OppoSecVectors
    add     x9, x9, :lo12:OppoSecVectors
    cmp     x24, #4
    b.eq    .LVectorEl1
    msr     vbar_el2, x9
    b       .LStack
.LVectorEl1:
    msr     vbar_el1, x9
.LStack:
    msr     SPSel, #1
    adrp    x9, .LOppoSecStackTop
    add     x9, x9, :lo12:.LOppoSecStackTop
    mov     sp, x9
    sub     sp, sp, #64
    stp     x20, x21, [sp, #0]
    stp     x22, x23, [sp, #16]
    lsr     x24, x24, #2
    stp     x24, x25, [sp, #32]
    mrs     x26, cntfrq_el0
    stp     x26, x27, [sp, #48]
    dsb     sy
    isb
    mov     x0, sp
    bl      OppoSecMain
    // Returning is unsupported until a platform reset routine is integrated.
.global OppoSecPark
OppoSecPark:
    wfe
    b       OppoSecPark
.size OppoSecEntry, . - OppoSecEntry

.balign 2048
.global OppoSecVectors
.type OppoSecVectors, %object
OppoSecVectors:
    .rept 16
    b       OppoSecPark
    .balign 128
    .endr
.size OppoSecVectors, . - OppoSecVectors

.section .bss.oppo_sec_stack, "aw", %nobits
.balign 16
.LOppoSecStack:
    .skip 16384
.LOppoSecStackTop:
.section .note.GNU-stack, "", %progbits
