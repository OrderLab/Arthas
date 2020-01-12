#include<stdio.h>

struct X {
	int a;
	int b;
};

void dump(int *x, int *y)
{
  printf("x=%d,y=%d\n", *x, *y);
}

int main()
{
	struct X x;
	int *Xa, *Xb;

	Xa = &x.a;
	Xb = &x.b;

  *Xa = 10;
  *Xb = 15;
  dump(Xa, Xb);

	return 0;
}
