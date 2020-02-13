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
	pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL, 0666);
	printf("create\n");
        if (pop == NULL)
        {
                perror(path);
                exit(1);
        }
	PMEMoid root = pmemobj_root(pop, sizeof(struct my_root));
	printf("root\n");

	struct my_root *rootp = pmemobj_direct(root);
	double *pmem_int_ptr;
	int *pmem_int_ptr2;
	//char * pmem_region_variable = (uint64_t)pop + 10;

        PMEMoid oid;
	PMEMoid oid2;
	TX_BEGIN(pop){
		oid = pmemobj_tx_zalloc(sizeof(double), 1);
		pmem_int_ptr = pmemobj_direct(oid);
		printf("address of pmemint is %p\n", pmem_int_ptr);
		*pmem_int_ptr = 0;
		oid2 = pmemobj_tx_zalloc(sizeof(int), 1);
		*pmem_int_ptr2 = 12;
		printf("address of pmemint2 is %p\n", pmem_int_ptr2);
	}TX_END

        /*TX_BEGIN(pop){
                pmemobj_tx_add_range_direct(pmem_int_ptr, sizeof(double));
                *pmem_int_ptr = 5;
                pmemobj_tx_add_range_direct(pmem_int_ptr, sizeof(double));
                *pmem_int_ptr = 6;
                pmemobj_tx_add_range_direct(pmem_int_ptr, sizeof(double));
		*pmem_int_ptr = 10;
                pmemobj_tx_add_range_direct(pmem_int_ptr2, sizeof(int));
		*pmem_int_ptr2 = 3;
		pmemobj_tx_abort(-1);
        }TX_END*/
	printf("ints are %f and %d\n", *pmem_int_ptr, *pmem_int_ptr2);
        int a;
        a = 30/(*pmem_int_ptr);
}

void read_hello_string(char *buf){

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
