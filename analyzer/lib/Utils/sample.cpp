#include "sample.h"

int compute_sample(int x)
{
  int sum = 0;
  for (int i = 0; i < x; i++)
    sum += i;
  return sum;
}

