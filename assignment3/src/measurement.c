#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <float.h>

extern int dependent(int n);
extern int independent(int n);

volatile int a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, x = 0, y = 0, z = 0, k = 0;

int main(int argc, char *argv[]) 
{
    if (argc < 2) 
    {
        printf("Usage: %s <n>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);

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

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    dependent(n);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent_dependent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("%f \n", time_spent_dependent);

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
    clock_gettime(CLOCK_MONOTONIC, &start);

    independent(n);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent_independent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("%f \n", time_spent_independent);

    printf("%f \n", time_spent_dependent/time_spent_independent);
    return 0;
}

int dependent(int n)
{
    volatile int result = 0;

    for(int i = 0; i < n; i++)
    {
        x = a + b;
        y = x + c;
        z = y + d;
        result += z;
    }

    return result;

}

int independent(int n)
{
    volatile int result = 0;

    for(int i = 0; i < n; i++)
    {
        x = a + b;
        y = c + d;
        z = e + f;
        result += k;
    }

    return result;
}