#include <errno.h>
#include <fcntl.h>
#include <libpmemobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Name of our layout in the pool
#define LAYOUT "hello_layout"

int main(int argc, char *argv[]){
  char *first_file = argv[1];
  char *second_file = argv[2];

  PMEMobjpool *first_pop, * second_pop;
  double *pmem_double_ptr;
  int *pmem_int_ptr;

  if(access(first_file, F_OK != 0) && access(second_file, F_OK != 0)){
    // Files don't exist
    first_pop = pmemobj_create(first_file, LAYOUT, PMEMOBJ_MIN_POOL, 0666);
    second_pop = pmemobj_create(second_file, LAYOUT, PMEMOBJ_MIN_POOL, 0666);
    PMEMoid oid, oid2;
    TX_BEGIN(first_pop){
       oid = pmemobj_tx_zalloc(sizeof(double), 1);
       pmem_double_ptr = pmemobj_direct(oid);
       *pmem_double_ptr = 1;
    }TX_END
    
   TX_BEGIN(first_pop){
     pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
     *pmem_double_ptr = 2;
   }TX_END

   TX_BEGIN(first_pop){
     pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
     *pmem_double_ptr = 3;
   }TX_END

   TX_BEGIN(first_pop){
     pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
   }TX_END

   TX_BEGIN(second_pop){
     oid = pmemobj_tx_zalloc(sizeof(int), 2);
     pmem_int_ptr = pmemobj_direct(oid);
     *pmem_int_ptr = 5;
   }TX_END

   TX_BEGIN(second_pop){
     pmemobj_tx_add_range_direct(pmem_int_ptr, sizeof(int));
     *pmem_int_ptr = 6;
   }TX_END

   TX_BEGIN(second_pop){
     pmemobj_tx_add_range_direct(pmem_int_ptr, sizeof(int));
     *pmem_int_ptr = 7;
   }TX_END

   TX_BEGIN(second_pop){
     *pmem_int_ptr = 7;
   }TX_END


  } 
  else{    
    first_pop = pmemobj_open(first_file, LAYOUT);
    second_pop = pmemobj_open(second_file, LAYOUT);

    PMEMoid oid = POBJ_FIRST_TYPE_NUM(first_pop, 1);
    while (oid.off) {
      pmem_double_ptr = pmemobj_direct(oid);
      printf("value of double is %f\n", *pmem_double_ptr);
      oid = POBJ_NEXT_TYPE_NUM(oid);
    }

    oid = POBJ_FIRST_TYPE_NUM(second_pop, 2);
    while (oid.off) {
      pmem_int_ptr = pmemobj_direct(oid);
      printf("value of int is %d\n", *pmem_int_ptr);
      oid = POBJ_NEXT_TYPE_NUM(oid);
    }
  }

  int a;
  if(*pmem_int_ptr == 7)
   a = 7 - *pmem_int_ptr;
  else
   a = 6 - *pmem_int_ptr;
  int b = 30 / a;
  printf("FINISHED!!! b is %d\n", b);

  return 0;
}
