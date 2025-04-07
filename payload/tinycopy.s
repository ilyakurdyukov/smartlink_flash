@ -*- tab-width: 8 -*-
	.code 16
	.arch armv7-m
	.syntax unified
	.section .text._start, "ax", %progbits
	.p2align 2
	.global _start
	.type _start, %function
_start:
.if 1
	// r4 = 0x800de8
	ldr	r2, [r4, #0x210] // data_len
	ldr	r1, [r4, #0x219] // cdb + 2
	strh	r2, [r4, #6]
	mov	r3, #0xb2a + 1 // memcpy
	add	r0, r4, #8
	bx	r3
.else
	adr	r3, 1f
	ldmia	r3, {r0-r3}
	strh	r2, [r0, #-2]
	bx	r3

	.p2align 2
1:	.long	0x800de8 + 8
	.long	0, 64 // addr, len
	.long	0xb2a + 1 // memcpy
.endif
