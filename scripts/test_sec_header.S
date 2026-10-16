/* SPDX-License-Identifier: BSD-2-Clause */
/* QEMU-only fixture: ARM64 image metadata followed by a direct SEC invocation.
 * No copying/relocation or second payload. Not an OPPO firmware image.
 */
.section .test_header, "ax", %progbits
.global TestImageStart
TestImageStart:
    b       TestStart
    nop
    .quad   0x80000
    .quad   TestImageEnd - TestImageStart
    .quad   0
    .quad   0
    .quad   0
    .quad   0
    .long   0x644d5241
    .long   0
TestStart:
    mov     x1, #0x1111
    mov     x2, #0x2222
    mov     x3, #0x3333
    b       OppoSecEntry
.section .note.GNU-stack, "", %progbits
