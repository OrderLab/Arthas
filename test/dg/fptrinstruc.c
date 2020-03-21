#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct test_type_t {
  char *type_name;
  int type;
  int (*func_p)(struct test_type_t *);
};

static int a_func(struct test_type_t *ttt) {
  printf("type name is %s\n", ttt->type_name);
  return ttt->type;
}

static struct test_type_t test_instance_2 = {
    .type_name = "instance 2", .type = 2, .func_p = a_func,
};

int main(int argc, char const *argv[]) {
  struct test_type_t *tt = &test_instance_2;
  int ty = tt->func_p(tt);
  printf("after calling the function pointer, type=%d\n", ty);
  return 0;
}
