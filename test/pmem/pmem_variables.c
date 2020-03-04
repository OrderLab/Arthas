#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <libpmemobj.h>

// Name of our layout in the pool
#define LAYOUT "hello_layout"

// Maximum length of our buffer
#define MAX_BUF_LEN 30
struct my_root {
        size_t len;
        char buf[MAX_BUF_LEN];
};

void write_hello_string(char *buf, char *path){
	PMEMobjpool *pop;
        printf("before create\n");
	pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL, 0666);
	printf("create\n");
        printf("POOL address:%p\n", pop);
        if (pop == NULL)
        {
                perror(path);
                exit(1);
        }
	PMEMoid root = pmemobj_root(pop, sizeof(struct my_root));
	printf("root\n");

	struct my_root *rootp = pmemobj_direct(root);
	double *pmem_double_ptr;
	int *pmem_int_ptr2;
	//char * pmem_region_variable = (uint64_t)pop + 10;
	
        int *pmem_int_ptrs[5];
        PMEMoid oid;
	PMEMoid oid2;
	TX_BEGIN(pop){
		oid = pmemobj_tx_zalloc(sizeof(double), 1);
		pmem_double_ptr = pmemobj_direct(oid);
		printf("address of pmem double is %p %ld\n", pmem_double_ptr, (uint64_t)pmem_double_ptr);
		*pmem_double_ptr = 3;
		oid2 = pmemobj_tx_zalloc(sizeof(int), 1);
		pmem_int_ptr2 = pmemobj_direct(oid2);
		*pmem_int_ptr2 = 12;
		printf("address of pmemint2 is %p %ld\n", pmem_int_ptr2, (uint64_t)pmem_int_ptr2);

                PMEMoid oid3;
                for(int i = 0; i < 5; i++){
                  oid3 = pmemobj_tx_zalloc(sizeof(int), 1);
                  pmem_int_ptrs[i] = pmemobj_direct(oid3);
                  *pmem_int_ptrs[i] = i+1;
                }
		printf("end of first transaction\n");
	}TX_END

        TX_BEGIN(pop){
                pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
                *pmem_double_ptr = 5;
                pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
                *pmem_double_ptr = 6;
                pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
		*pmem_double_ptr = 10;
                pmemobj_tx_add_range_direct(pmem_int_ptr2, sizeof(int));
		*pmem_int_ptr2 = 13;
		pmemobj_tx_add_range_direct(pmem_int_ptrs[0], sizeof(int)*5);
		//pmemobj_tx_abort(-1);
        }TX_END

        /*TX_BEGIN(pop){
                pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
               *pmem_double_ptr = 11;
                pmemobj_tx_add_range_direct(pmem_int_ptr2, sizeof(int));
		*pmem_int_ptr2 = 4;
        }TX_END

	TX_BEGIN(pop){
                pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
               *pmem_double_ptr = 8;
	}TX_END

	TX_BEGIN(pop){
                pmemobj_tx_add_range_direct(pmem_double_ptr, sizeof(double));
               *pmem_double_ptr = 50;
	}TX_END*/

	printf("%p %p\n", pmem_double_ptr, pmem_int_ptr2);
	printf("ints are %f and %d\n", *pmem_double_ptr, *pmem_int_ptr2);
        //int a;
	//int b = *pmem_double_ptr;
        //a = 30/(b);
}

void read_hello_string(char *buf){
  printf("Trying to open pmem file %s\n", buf);
  PMEMobjpool *pop = pmemobj_open(buf, LAYOUT);
  if(pop == NULL){
    perror(buf);
    exit(1);
  }
}
int main(int argc, char *argv[])
{
        char *path = argv[2];

        // Create the string to save to persistent memory
        char buf[MAX_BUF_LEN] = "Hello Persistent Memory";

        if (strcmp (argv[1], "-w") == 0) {

                write_hello_string(buf, path);

        } else if (strcmp (argv[1], "-r") == 0) {

                read_hello_string(path);
        } else {
                exit(1);
        }

}
