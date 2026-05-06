#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <chrono>

extern "C" {
  void benchmark_c(	float const * i_a,
               		float const * i_b );
  void benchmark_asm_1( float const * i_a,
                 		float const * i_b );
  void benchmark_asm_2( float const * i_a,
                 		float const * i_b );
}

void benchmark_inline_asm_1( float const * i_a,
             				 float const * i_b ){
	for(long i = 0; i < 10000000000; i+=4){
		asm (           
			"ld1 {v2.4s}, [%1];"			
			"ld1 {v1.4s}, [%0];"			

			//TODO: 
			//Use the FMLA instruction 
			//see: https://developer.arm.com/documentation/ddi0602/2025-03/SIMD-FP-Instructions/FMLA--vector---Floating-point-fused-multiply-add-to-accumulator--vector--?lang=en

			: 													//no output registers have to be read
			: 	"r"(&i_a[0]), 
				"r"(&i_b[0])									//input registers linked to variables "f" = floating point register, "r" = register
			:   "v4", "v3", "v2", "v1"	
																//clobbers = all used SIMD registers and memory (pointed to by input registers)
		);
	}
	return;
}

void benchmark_inline_asm_2( float const * i_a,
             				 float const * i_b ){
	asm (      
		//TODO: 
		//Insert the Kernel Code from benchmark_asm_1.s
		"nop;"
		: 								//output registers 
		: 								//input registers 
		:  								//clobbers = all used/ dirty SIMD registers and memory (pointed to by input registers)
	);
	return;
}



int main() {
  float l_a[4] = { 1.0f, 1.0f, 1.0f, 1.0f};
  float l_b[4] = { 1.0f, 1.0f, 1.0f, 1.0f};
  std::chrono::steady_clock::time_point start, stop;
  std::chrono::duration<double> duration1, duration2, duration3, duration4, duration5;
  double FLOPS = 0;
  long repetitions = 10000000000;

  // benchmark_c
  std::cout << "### calling benchmark_c ###" << std::endl;
  start = std::chrono::steady_clock::now();
  benchmark_c( l_a, l_b );
  stop = std::chrono::steady_clock::now();
  duration1 = std::chrono::duration_cast< std::chrono::duration< double> >( stop - start );

  // benchmark_asm_1
  std::cout << "### calling benchmark_asm_1 ###" << std::endl;
  start = std::chrono::steady_clock::now();
  benchmark_asm_1( l_a, l_b );
  stop = std::chrono::steady_clock::now();
  duration2 = std::chrono::duration_cast< std::chrono::duration< double> >( stop - start );

  // benchmark_asm_2
  std::cout << "### calling benchmark_asm_2 ###" << std::endl;
  start = std::chrono::steady_clock::now();
  benchmark_asm_2( l_a, l_b );
  stop = std::chrono::steady_clock::now();
  duration3 = std::chrono::duration_cast< std::chrono::duration< double> >( stop - start );  

  // benchmark_inline_asm_1
  std::cout << "### calling benchmark_inline_asm ###" << std::endl;
  start = std::chrono::steady_clock::now();
  benchmark_inline_asm_1( l_a, l_b );
  stop = std::chrono::steady_clock::now();
  duration4 = std::chrono::duration_cast< std::chrono::duration< double> >( stop - start );  

  // benchmark_inline_asm_2
  std::cout << "### calling benchmark_inline_asm ###" << std::endl;
  start = std::chrono::steady_clock::now();
  benchmark_inline_asm_2( l_a, l_b );
  stop = std::chrono::steady_clock::now();
  duration5 = std::chrono::duration_cast< std::chrono::duration< double> >( stop - start );  

  std::cout << "Duration C: " << duration1.count() << " seconds" << std::endl;
  std::cout << "Duration ASM SIMD: " << duration2.count() << " seconds" << std::endl;
  std::cout << "Duration ASM No SIMD: " << duration3.count() << " seconds" << std::endl;
  std::cout << "Duration Inline ASM SIMD (Q-Form): " << duration4.count() << " seconds" << std::endl;
  std::cout << "Duration Inline ASM SIMD (D-Form): " << duration5.count() << " seconds" << std::endl;

  return EXIT_SUCCESS;
}
