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
	PMEMoid root = pmemobj_root(pop, sizeof(struct my_root));
	struct my_root *rootp = pmemobj_direct(root);
	int *pmem_int_ptr;
	TX_BEGIN(pop){
		PMEMoid oid;
		oid = pmemobj_tx_zalloc(sizeof(int), 1);
		pmem_int_ptr = pmemobj_direct(oid);
		*pmem_int_ptr = 3;
	}TX_END
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
