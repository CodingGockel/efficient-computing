#include <stdint.h>

void benchmark_c( 	float const *i_a,
             		float const *i_b ) {

	long repeat = 10000000000;	
	float res[4];

	while(repeat){

		res[0] = i_a[0] * i_b[0] + res[0];
		res[1] = i_a[1] * i_b[1] + res[1];
		res[2] = i_a[2] * i_b[2] + res[2];
		res[3] = i_a[3] * i_b[3] + res[3];

		repeat-=4;

	}
}
