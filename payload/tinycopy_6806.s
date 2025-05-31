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
	// r4 = 0x800df8
	ldr	r2, [r4, #0x14] // data_len
	ldr	r1, [r4, #0x1d] // cdb + 2
	strh	r2, [r4, #6]
	mov	r3, #0xc8e + 1 // memcpy
	ldr	r0, [r4, #8]
	bx	r3
.elseif 1 // write mem
	// r4 = 0x800df8
	movs	r2, #0xff
	movs	r3, #0
	strh	r3, [r4, #6]
	ldr	r0, [r4, #0x1d] // cdb + 2
	mov	r3, #0xc8e + 1 // memcpy
	adr	r1, 9f
	bx	r3
	.p2align 2
9:
.elseif 1 // dump lr (0x6617)
	ldr	r0, 1f
	movs	r2, #4
	strh	r2, [r0, #6]
	ldr	r0, [r0, #8]
	str	lr, [r0]
	bx	lr

	.p2align 2
1:	.long	0x800df8
.else
	adr	r3, 1f
	ldmia	r3, {r0-r3}
	strh	r2, [r0, #6]
	ldr	r0, [r0, #8]
	bx	r3

	.p2align 2
1:	.long	0x800df8
	.long	0, 64 // addr, len
	.long	0xc8e + 1 // memcpy
.endif

