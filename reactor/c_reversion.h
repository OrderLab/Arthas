#ifndef C_REVERSION_H
#define C_REVERSION_H 1

#include <stdio.h>
#include "libpmemobj.h"
#include <stdlib.h>
#include  <sys/wait.h>

#define MAX_COARSE_ATTEMPTS 5
#define MAX_DATA 1000

#define MAX_VARIABLES 1000
#define MAX_VERSIONS 3

#define INT_CHECKPOINT 0
#define DOUBLE_CHECKPOINT 1
#define STRING_CHECKPOINT 2
#define BOOL_CHECKPOINT 3

struct checkpoint_data {
  const void *address;
  uint64_t offset;
  void *data[MAX_VERSIONS];
  size_t size[MAX_VERSIONS];
  int version;
  int data_type;
};

struct checkpoint_log{
  struct checkpoint_data c_data[MAX_VARIABLES];
  int variable_count;
};

struct checkpoint_log * reconstruct_checkpoint(void);

void coarse_grain_reversion(void ** addresses, struct checkpoint_log *c_log,
  void **pmem_addresses, int version_num, int num_data);

PMEMobjpool * redo_pmem_addresses(char *path, char *layout, int num_data,
  void **pmem_addresses, uint64_t * offsets);

void re_execute(char **reexecution_lines, int version_num, int line_counter,
  void ** addresses, struct checkpoint_log *c_log, void **pmem_addresses, int num_data,
  char * path, char * layout, uint64_t *offsets);

int main_func(int argc, char *argv[]);

void revert_by_address(const void *search_address, const void *address, int variable_index, int version, int type, size_t size, struct checkpoint_log * c_log);

int search_for_address(const void * address, size_t size, struct checkpoint_log * c_log);

#endif
