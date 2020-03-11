#include <errno.h>
#include <fcntl.h>
#include <libpmemobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Name of our layout in the pool
#define LAYOUT "hello_layout"

// Maximum length of our buffer
#define MAX_BUF_LEN 30
struct my_root {
  size_t len;
  char buf[MAX_BUF_LEN];
};

int handle_behavior(char *path) {
  PMEMobjpool *pop;
  double *pmem_double_ptr;
  int *pmem_int_ptr2;
  int *pmem_int_ptrs[5];

  if (access(path, F_OK != 0)) {
    // File does not exist
    pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL, 0666);
    printf("POOL address:%p\n", pop);
    if (pop == NULL) {
      perror(path);
      exit(1);
    }
    // Create variables
    PMEMoid oid, oid2;
    TX_BEGIN(pop) {
      oid = pmemobj_tx_zalloc(sizeof(double), 1);
      pmem_double_ptr = pmemobj_direct(oid);
      printf("address of pmem double is %p %ld\n", pmem_double_ptr,
             (uint64_t)pmem_double_ptr);
      *pmem_double_ptr = 3;
      oid2 = pmemobj_tx_zalloc(sizeof(int), 2);
      pmem_int_ptr2 = pmemobj_direct(oid2);
      *pmem_int_ptr2 = 12;
      printf("address of pmemint2 is %p %ld\n", pmem_int_ptr2,
             (uint64_t)pmem_int_ptr2);

      PMEMoid oid3;
      for (int i = 0; i < 5; i++) {
        oid3 = pmemobj_tx_zalloc(sizeof(int), 2);
        pmem_int_ptrs[i] = pmemobj_direct(oid3);
        *pmem_int_ptrs[i] = i + 1;
      }
    }
    TX_END

    TX_BEGIN(pop) {
      pmemobj_tx_add_range_direct(pmem_int_ptr2, sizeof(int));
      *pmem_int_ptr2 = 13;
      pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
      *pmem_double_ptr = 10;
      pmemobj_tx_add_range_direct(pmem_int_ptrs[0], sizeof(int) * 5);
    }
    TX_END

    TX_BEGIN(pop) {
      pmemobj_tx_add_range_direct(pmem_int_ptr2, sizeof(int));
      *pmem_int_ptr2 = 4;
      pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
      *pmem_double_ptr = 11;
    }
    TX_END

    TX_BEGIN(pop) {
      pmemobj_tx_add_range_direct(pmem_int_ptr2, sizeof(int));
      pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
    }
    TX_END
  } else {
    // File exists, open and recover.
    PMEMobjpool *pop = pmemobj_open(path, LAYOUT);
    if (pop == NULL) {
      perror(path);
      exit(1);
    }
    PMEMoid oid3;

    // Recover data variablesDoubles are of type 1, Ints are type 2.
    PMEMoid oid = POBJ_FIRST_TYPE_NUM(pop, 1);
    while (oid.off) {
      pmem_double_ptr = pmemobj_direct(oid);
      printf("value of double is %f\n", *pmem_double_ptr);
      oid = POBJ_NEXT_TYPE_NUM(oid);
    }

    oid = POBJ_FIRST_TYPE_NUM(pop, 2);
    int *i;
    int count = 0;
    while (oid.off) {
      // i = pmemobj_direct(oid);
      // printf("value of int is %d\n", *i);
      if (count == 0) {
        pmem_int_ptr2 = pmemobj_direct(oid);
        printf("value of int is %d\n", *pmem_int_ptr2);
      } else {
        pmem_int_ptrs[count - 1] = pmemobj_direct(oid);
        printf("value of int is %d\n", *pmem_int_ptrs[count - 1]);
      }
      count++;
      oid = POBJ_NEXT_TYPE_NUM(oid);
    }
  }

  // Now we will do calculations with variables:
  int a;
  if (*pmem_int_ptr2 == 4)
    a = 4 - *pmem_int_ptr2;
  else
    a = 13 - *pmem_int_ptr2;
  int b = 30 / a;
  printf("FINISHED!!! b is %d\n", b);
  return 1;
}

int main(int argc, char *argv[]) {
  char *path = argv[1];

  return handle_behavior(path);
}
