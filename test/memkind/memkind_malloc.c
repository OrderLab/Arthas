#include <memkind.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define PMEM_MAX_SIZE (1024 * 1024 * 32)

static char path[PATH_MAX]="/tmp/";

int main(int argc, char ** argv){
	struct memkind *pmem_kind = NULL;
	int err = 0;
	
	if (argc > 2) {
        	fprintf(stderr, "Usage: %s [pmem_kind_dir_path]\n", argv[0]);
        	return 1;
	} else if (argc == 2 && (realpath(argv[1], path) == NULL)) {
		fprintf(stderr, "Incorrect pmem_kind_dir_path %s\n", argv[1]);
        	return 1;
    	}

	err = memkind_create_pmem(path, PMEM_MAX_SIZE, &pmem_kind);
	if (err) {
        	return 1;
    	}

    	char *pmem_str1 = NULL;
	char *default_str = NULL;

	// Allocate 512 Bytes of 32 MB available (Persistent Memory)
    	pmem_str1 = (char *)memkind_malloc(pmem_kind, 512);
    	if (pmem_str1 == NULL) {
        	fprintf(stderr, "Unable to allocate pmem string (pmem_str1).\n");
        	return 1;
    	}
	
	// Allocate Non Persistent Memory
	default_str = (char *)memkind_malloc(MEMKIND_DEFAULT, 512);
    	if (default_str == NULL) {
        	perror("memkind_malloc()");
        	fprintf(stderr, "Unable to allocate default string\n");
		return 1;
    	}
}
