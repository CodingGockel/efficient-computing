	.arch armv8-a
	.file	"benchmark_c.c"
	.text
	.align	2
	.global	benchmark_c
	.type	benchmark_c, %function
benchmark_c:
.LFB0:
	.cfi_startproc
	sub	sp, sp, #48
	.cfi_def_cfa_offset 48
	str	x0, [sp, 8]
	str	x1, [sp]
	mov	x0, 58368
	movk	x0, 0x540b, lsl 16
	movk	x0, 0x2, lsl 32
	str	x0, [sp, 40]
	b	.L2
.L3:
	ldr	x0, [sp, 8]
	ldr	s30, [x0]
	ldr	x0, [sp]
	ldr	s31, [x0]
	fmul	s30, s30, s31
	ldr	s31, [sp, 24]
	fadd	s31, s30, s31
	str	s31, [sp, 24]
	ldr	x0, [sp, 8]
	add	x0, x0, 4
	ldr	s30, [x0]
	ldr	x0, [sp]
	add	x0, x0, 4
	ldr	s31, [x0]
	fmul	s30, s30, s31
	ldr	s31, [sp, 28]
	fadd	s31, s30, s31
	str	s31, [sp, 28]
	ldr	x0, [sp, 8]
	add	x0, x0, 8
	ldr	s30, [x0]
	ldr	x0, [sp]
	add	x0, x0, 8
	ldr	s31, [x0]
	fmul	s30, s30, s31
	ldr	s31, [sp, 32]
	fadd	s31, s30, s31
	str	s31, [sp, 32]
	ldr	x0, [sp, 8]
	add	x0, x0, 12
	ldr	s30, [x0]
	ldr	x0, [sp]
	add	x0, x0, 12
	ldr	s31, [x0]
	fmul	s30, s30, s31
	ldr	s31, [sp, 36]
	fadd	s31, s30, s31
	str	s31, [sp, 36]
	ldr	x0, [sp, 40]
	sub	x0, x0, #4
	str	x0, [sp, 40]
.L2:
	ldr	x0, [sp, 40]
	cmp	x0, 0
	bne	.L3
	nop
	nop
	add	sp, sp, 48
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
.LFE0:
	.size	benchmark_c, .-benchmark_c
	.ident	"GCC: (GNU) 14.3.0"
	.section	.note.GNU-stack,"",@progbits
