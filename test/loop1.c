#include<stdio.h>
#include<stdlib.h>

int foo(int c)
{
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 3; j++) {
          for (int k = 0; k < 5; k++) {
            c += i;
            c *= j;
          }
        }
        if (c > 100)
          return c;
    }
    c++;
    return c;
}

int add(int n)
{
    int i;
    int sum = 0;
    for (i = 1; i * i < n; i++) {
       sum += i; 
    }
    if (sum > 10000)
      return 10000;
    return sum;
}

unsigned long mul(int n)
{
  unsigned long product = 1;
  for (int i = 2; i < n; i++) {
    product *= i;
  }
  return product;
}

int bar(int n)
{
  int ret;
  if (n > 10)
    ret = 10;
  else
    ret = n / 2;
  return ret;
}

int main(int argc, char *argv[])
{
  int n, sum;
  unsigned long result;
  if (argc > 1) {
    n = atoi(argv[1]);
  } else {
    printf("Enter input: ");
    scanf("%d", &n);
  }
  sum = add(n);
  result = foo(sum);
  result = mul(bar(result));
  printf("result for input %d is %lu\n", n, result);
}
