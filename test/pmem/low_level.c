#define _GNU_SOURCE

//#include "../../../../../memkind/include/memkind.h"
#include <emmintrin.h>
#include <errno.h>
#include <fcntl.h>
#include <fcntl.h>
#include <memkind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <xmmintrin.h>
#include "/home/brian/a_bug_project/feb17/1.8-PMDK-Arthas/src/libpmem/checkpoint_hashmap.h"
#include "libpmem.h"
#include "libpmemobj.h"
#define PMEM_MAX_SIZE (1024 * 1042 * 32)

static void print_err_message(int err) {
  char error_message[MEMKIND_ERROR_MESSAGE_SIZE];
  memkind_error_message(err, error_message, MEMKIND_ERROR_MESSAGE_SIZE);
  fprintf(stderr, "%s\n", error_message);
}
/*int main(int argc, char *argv[]){
  int fd;
  struct stat stbuf;
  char *pmaddr;
  if ((fd = open(argv[1], O_RDWR | O_CREAT)) < 0)
    printf("file not opened\n");
  if (fstat(fd, &stbuf) < 0)
    printf("fstat failed\n");
  if ((pmaddr = mmap(NULL, stbuf.st_size,
                PROT_READ|PROT_WRITE,
                MAP_SHARED, fd, 0)) == MAP_FAILED)
      printf("failure mmap\n");
  close(fd);
  strcpy(pmaddr, "This is new data written to the file");

}*/
int main(int argc, char *argv[]) {
  char *path = argv[1];
  if (access(path, F_OK) != 0) {
    int fd = open(path, O_CREAT | O_RDWR);
    if (fd == -1) printf("file creation did not work for some reason\n");

    fallocate(fd, 0, 0, 1000);
   // char *data =
   //     (char *)mmap(NULL, 100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    void *data = 
        mmap(NULL, 100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("data is %p\n", data);
    if (data == MAP_FAILED) printf("map has failed \n");
    printf("before data\n");
    int a = 3;
    memcpy(data, &a, sizeof(int));
   // strcpy(data, "hello");
    printf("after data clflush pointer is %p\n", data);
    _mm_clflush(data);
    a = 4;
    memcpy(data, &a, sizeof(int));
    _mm_clflush(data);
    a = 5;
    memcpy(data, &a, sizeof(int));
    _mm_clflush(data);
    _mm_sfence();

    int c;
    if(*((int *)data) == 5)
      c = 5 - *((int *)data);
    else
      c = 4 - *((int *)data);
    int b = 30 / c;
    printf("finished %d\n", *((int *)data));
  }

  else{
    printf("file exists\n");
    int fd = open(path, O_CREAT | O_RDWR);
    perror("open");
    if (fd == -1) printf("file creation did not work for some reason\n");
    void *data =
        mmap(NULL, 100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("data value is %d\n", *((int *)data));
  }
}

/*int main(int argc, char *argv[]){
  struct memkind *pmem_kind;
  int err = 0;
  char *path = argv[1];
  printf("path is %s\n", path);
  //init_checkpoint_log();
  err = memkind_create_pmem(path, PMEM_MAX_SIZE, &pmem_kind);
  if (err) {
    char *msg;
    print_err_message(err);
    return 0;
  }
  char *pmem_str = NULL;
  size_t mapped_len;
  int is_pmem;
  pmem_str = (char *)memkind_malloc(pmem_kind, 512);
  //pmem_str = (char *)pmem_map_file(path, 1024, PMEM_FILE_CREATE,
  //                    0666, &mapped_len, &is_pmem);
  //pmem_str = malloc(32);
  //PMEMobjpool *pop = pmemobj_create(path, "layout", PMEMOBJ_MIN_POOL,
  //                                  066);
  printf("pme str malloc is %p\n", pmem_str);
  if(pmem_str == NULL){
    printf("bad pmem malloc\n");
  }
  strcpy(pmem_str, "hello");
  _mm_clflush(pmem_str);
  _mm_sfence();
  printf("finished\n");
  memkind_free(pmem_kind, pmem_str);

}*/
