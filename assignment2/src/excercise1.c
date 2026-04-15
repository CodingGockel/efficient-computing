#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <float.h>

extern double randInRange(double min, double max);

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <n>\n", argv[0]);
    return 1;
  }
  int n = atoi(argv[1]);

  double x[n];
  double y[n];
  double a = randInRange(DBL_MIN, DBL_MAX);

  for(int i = 0; i < n; i++)
  {
    x[i] = randInRange(DBL_MIN, DBL_MAX);
    y[i] = randInRange(DBL_MIN, DBL_MAX);
  }

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  for(int i = 0; i< n; i++)
  {
    y[i] = (a * x[i]) + y[i];
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