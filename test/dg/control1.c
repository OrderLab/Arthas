#include <assert.h>
#include <stdio.h>
int main(void) {
  int a = 0xdeadbeef;
  int b = 0;
  int c = 1;

  while (c) {
    if (b == 0) goto L;

    assert(a == 1);
    if (a == 1) {
      c = 0;
    L:
      a = 1;
      b = 1;
    }
  }
  printf("a=%d\n", a);
  return 0;
}
