#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libpmemobj.h>

#define LAYOUT_NAME "pmem_vector"

struct vector {
	int x;
	int y;
	int z;
};

void read_vector(PMEMobjpool *pop)
{
  PMEMoid root;
	struct vector *vp;

	root = pmemobj_root(pop, sizeof (struct vector));
	vp = pmemobj_direct(root);
	printf("vector.x=%d, vector.y=%d, vector.z=%d\n", vp->x, vp->y, vp->z);
}

void write_vector(PMEMobjpool *pop, int x, int y, int z)
{
	PMEMoid root;
	struct vector *vectorp;

	root = pmemobj_root(pop, sizeof (struct vector));
	vectorp = pmemobj_direct(root);
	TX_BEGIN(pop) {
		pmemobj_tx_add_range(root, 0, sizeof(struct vector));
		vectorp->x = x;
		vectorp->y = y;
		vectorp->z = z;
	} TX_END
}

void usage(char *prog)
{
  fprintf(stderr, "Usage: %s <pmem file> <write|read> [num1 num2 num3]\n", prog);
}

int main(int argc, char *argv[])
{
	PMEMobjpool *pop;
	if (argc < 3) {
    usage(argv[0]);
		return 1;
	}
  if (strcmp(argv[2], "write") == 0) {
    if (argc < 6) {
      usage(argv[0]);
		  return 1;
    }
		pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL, 0666);
		if (pop == NULL) {
			perror("pmemobj_create");
			return 2;
		}
    write_vector(pop, atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
	  pmemobj_close(pop);
	} else if (strcmp(argv[2], "read") == 0) {
		pop = pmemobj_open(argv[1], LAYOUT_NAME);
		if (pop == NULL) {
			perror("pmemobj_create");
			return 3;
		}
    read_vector(pop);
	  pmemobj_close(pop);
	} else {
    usage(argv[0]);
    return 1;
  }
	return 0;
}
