#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <float.h>

extern double randInRange(double min, double max);

int main(int argc, char *argv[]) {
  if (argc < 4) {
    printf("Usage: %s <n> <m> <l>\n", argv[0]);
    return 1;
  }
  int n = atoi(argv[1]);
  int m = atoi(argv[2]);
  int l = atoi(argv[3]);

  double A[n][m];
  double B[m][l];
  double C[n][l];
  
  for(int i = 0; i < n; i++)
  {
    for(int j = 0; j < m; j++)
    {
        A[i][j] = randInRange(DBL_MIN, DBL_MAX);
    }
  }

  for(int i = 0; i < m; i++)
  {
    for(int j = 0; j < l; j++)
    {
        B[i][j] = randInRange(DBL_MIN, DBL_MAX);
    }
  }

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  // HERE GOES THE KERNEL

  for(int i = 0; i < n; i++)
  {
    for(int j = 0; j < m; j++)
    {
        for(int k = 0; k < l; k++)
        {
            C[i][k] += A[i][j] * B[j][k];
        }
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &end);
  double time_spent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
  printf("%f", time_spent);
  return 0;
}

double randInRange(double min, double max) 
{
    double range = (max - min); 
    double div = RAND_MAX / range;
    return min + (rand() / div);
}