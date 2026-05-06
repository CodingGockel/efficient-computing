        .text
        .align 4
        .type   benchmark_asm_1, %function
        .global benchmark_asm_1
benchmark_asm_1:

		ldr x4, =10000000000

		ld1 {v2.4s}, [x1]			//v2: i_b
		ld1 {v1.4s}, [x0]			//v1: i_a

while:	cmp x4, xzr					//while i > 0

		b.eq finish

		//TODO: 
		//Use the FMLA instruction 
		//see: https://developer.arm.com/documentation/ddi0602/2025-03/SIMD-FP-Instructions/FMLA--vector---Floating-point-fused-multiply-add-to-accumulator--vector--?lang=en

		sub x4, x4, #4			//decrement counter, next 4 operations
		b while
finish:
        ret
        .size   benchmark_asm_1, (. - benchmark_asm_1)
