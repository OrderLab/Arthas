#include<stdio.h>

struct Pair {
	int a;
	int b;
};

void dump(int *x, int *y)
{
  printf("x=%d,y=%d\n", *x, *y);
}

void process(int x, int y)
{
	struct Pair p;
	int *Pa, *Pb;

	Pa = &p.a;
	Pb = &p.b;

  *Pa = x;
  *Pb = y;
  dump(Pa, Pb);
}

int main()
{
  process(10, 15);
	return 0;
}
