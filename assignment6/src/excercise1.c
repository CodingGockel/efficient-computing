#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

int* snipped_1_parallel(int *x, int *y, int *A, int n, int m);

int main(int argc, char *argv[]) {

    if (argc < 4) 
    {
        printf("Usage: %s <n> <m> <k>\n", argv[0]);
        return 1;
    }

    size_t n = atoi(argv[1]);     
    size_t m = atoi(argv[2]); 
    size_t k = atoi(argv[3]);  // Not used in this snippet

    int *x = malloc(n * sizeof(int));
    int *y = malloc(m * sizeof(int));
    int *A = malloc(n * m * sizeof(int)); // flat 2D array

    for(int i = 0; i < n; i++)
    {
        x[i] = i;
        for(int j = 0; j < m; j++)
        {
            A[i * m + j] = i + j;  // access via flat array
        }
    }

    int *y_result = snipped_1_parallel(x, y, A, n, m);

    for(int i=0; i < m; i++)
    {
        printf("%d,",y_result[i]);
    }
    printf("\n");

    free(x);
    free(y);
    free(A);

    return 0;
}

int* snipped_1_parallel(int *x, int *y, int *A, int n, int m)
{
    #pragma omp parallel for
    for (int i = 0; i < m; i++){
        y[i] = 0;
        #pragma omp parallel for
        for (int j = 0; j < n; j++){
            y[i] += A[j * m + i] * x[j];  // Note the indexing: column-major
        }
    }
    return y;
}