#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <float.h>

extern double randInRange(double min, double max);

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s <n> <m>\n", argv[0]);
    return 1;
  }
  int n = atoi(argv[1]);
  int m = atoi(argv[2]);
 
  double x[n];
  double y[m];
  double a[m][n];

  for(int i = 0; i < n; i++)
  {
    x[i] = randInRange(DBL_MIN, DBL_MAX);
  }

  for(int i = 0; i < m; i++)
  {
    for(int j = 0; j < n; j++)
    {
        a[i][j] = randInRange(DBL_MIN, DBL_MAX);
    }
  }

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  for(int i = 0; i < m; i++)
  {
    for(int j = 0; j < n; j++)
    {
        y[i] += a[i][j] * x[j]; 
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