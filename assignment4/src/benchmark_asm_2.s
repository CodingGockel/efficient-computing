        .text
        .align 4
        .type   benchmark_asm_2, %function
        .global benchmark_asm_2
benchmark_asm_2:

		ldr x4, =10000000000

		ldr s2, [x1]				//s2: i_b[0]
		ldr s1, [x0]				//s1: i_a[0]

		ldr s4, [x1, #4]			//s2: i_b[1]
		ldr s3, [x0, #4]			//s1: i_a[1]

		ldr s6, [x1, #8]			//s2: i_b[2]
		ldr s5, [x0, #8]			//s1: i_a[2]

		ldr s8, [x1, #12]			//s2: i_b[3]
		ldr s7, [x0, #12]			//s1: i_a[3]

while:	cmp x4, xzr					//while i > 0

		b.eq finish

		fmadd s9, s1, s2, s9		//s9 = s1 * s2  + s9
		fmadd s10, s3, s4, s10		//s10 = s3 * s4 + s10
		fmadd s11, s5, s6, s11		//s11 = s5 * s6 + s11
		fmadd s12, s7, s8, s12		//s12 = s7 * s8 + s12
	
		sub x4, x4, #4			//decrement counter, next operations
		b while
finish:
        ret
        ret
        .size   benchmark_asm_2, (. - benchmark_asm_2)
