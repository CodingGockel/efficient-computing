#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

void initialize_array(int *x, size_t n, size_t k) {
    for (size_t i = 0; i < n; i++) {
        x[i] = (i + k) % n;
    }
}

size_t pointer_chase(int *x, size_t start, size_t m) {
    size_t index = start;
    for (size_t i = 0; i < m; i++) {
        index = x[index];
    }
    return index;
}

int main(int argc, char *argv[]) {

    if (argc < 4) 
    {
        printf("Usage: %s <n>\n", argv[0]);
        return 1;
    }

    size_t n = atoi(argv[1]); 
    size_t k = atoi(argv[2]);      
    size_t m = atoi(argv[3]); 

    int *x = malloc(n * sizeof(int));
    if (!x) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    initialize_array(x, n, k);

    double A[100][100];
    double B[100][100];

    for(int i = 0; i < 100; i++)
    {
        for(int j = 0; j < 100; j++)
        {
            A[i][j] = 0.0;
        }
    }

    for(int i = 0; i < 100; i++)
    {
        for(int j = 0; j < 100; j++)
        {
            B[i][j] = 0.0;
        }
    }

    clock_t start_time = clock();
    size_t result = pointer_chase(x, 0, m);
    clock_t end_time = clock();

    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    printf("Pointer chasing finished. Result: %zu\n", result);
    printf("Time elapsed: %f seconds\n", elapsed);

    free(x);
    return 0;
}