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
	b	scsi_main
.else
	push	{r4,lr}
	ldr	r4, 1f
	add	r4, pc
	blx	r4
2:	pop	{r4,pc}
1:	.long	scsi_main - 2b
.endif
