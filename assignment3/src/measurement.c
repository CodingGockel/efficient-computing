#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <float.h>
#include <int.h>

extern double randInRange(double min, double max);
extern int randInRange(int min, int max);
extern int dependent(n);
extern int independent(n);

int main(int argc, char *argv[]) 
{
  if (argc < 2) 
  {
    printf("Usage: %s <n>\n", argv[0]);
    return 1;
  }

  int n = atoi(argv[1]);



}

int dependent(int n)
{
    return 0;
}

int independent(int n)
{
    int a,b,c,d,x,y,z = randInRange()
}

double randInRange(double min, double max) 
{
    double range = (max - min); 
    double div = RAND_MAX / range;
    return min + (rand() / div);
}

int randInRange(int min, int max) 
{
    int range = (max - min); 
    int div = RAND_MAX / range;
    return min + (rand() / div);
}