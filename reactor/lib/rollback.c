// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "rollback.h"

// C Implementation of Reverter because libpmemobj in c++
// expected dated pmem file version. Unable to backwards convert

int coarse_grained_tries = 0;
int fine_grained_tries = 0;

int search_for_address(const void *address, size_t size,
                       struct checkpoint_log *c_log) {
  uint64_t uint_address = (uint64_t)address;
  uint64_t search_upper_bound = uint_address + size;
  uint64_t clog_upper_bound;
  for (int i = 0; i < c_log->variable_count; i++) {
    uint64_t clog_address = (uint64_t)c_log->c_data[i].address;
    // Get size of first checkpointed data structure, should I iterate through
    // each size?
    clog_upper_bound =
        (uint64_t)clog_address + (uint64_t)c_log->c_data[i].size[0];
    // printf("size is %ld\n", (uint64_t)c_log->c_data[i].size[0]);
    // printf("uint_address %ld, clog_address %ld, search_upper_bound %ld,
    // clog_upper_bound %ld\n",
    // uint_address, clog_address, search_upper_bound, clog_upper_bound );
    if (uint_address >= clog_address &&
        search_upper_bound <= clog_upper_bound) {
      return i;
    }
    /*if(c_log->c_data[i].address == address){
      return i;
      }*/
  }
  return -1;
}

void revert_by_sequence_number(void **sorted_pmem_addresses, single_data *ordered_data,
                               int seq_num, int rollback_version){
  memcpy(sorted_pmem_addresses[seq_num], ordered_data[seq_num].old_data[rollback_version]
  , ordered_data[seq_num].old_size[rollback_version]);
}

void sort_by_sequence_number(void **addresses, single_data *ordered_data,
                             size_t total_size, int num_data,
                             void ** sorted_addresses,
                             void **pmem_addresses,
                             void **sorted_pmem_addresses){
  for(int i = 0; i < total_size; i++){
    for(int j = 0; j < num_data; j++){
      if(ordered_data[i].address == addresses[j]){
        sorted_addresses[i] = ordered_data[i].address;
        sorted_pmem_addresses[i] = pmem_addresses[j];
      }
    }
  }
}

void revert_by_address(const void *search_address, const void *address,
                       int variable_index, int version, int type, size_t size,
                       struct checkpoint_log *c_log) {
  void *dest = (void *)address;
  if (search_address == c_log->c_data[variable_index].address) {
    memcpy(dest, c_log->c_data[variable_index].data[version],
           c_log->c_data[variable_index].size[version]);
  } else {
    uint64_t uint_address = (uint64_t)search_address;
    uint64_t address_num = (uint64_t)c_log->c_data[variable_index].address;
    uint64_t offset = uint_address - address_num;
    memcpy(dest,
           (void *)((uint64_t)c_log->c_data[variable_index].data[version] +
                    offset),
           size);
  }
}

void coarse_grain_reversion(void **addresses, struct checkpoint_log *c_log,
                            void **pmem_addresses, int version_num,
                            int num_data) {
  int c_data_indices[MAX_DATA];
  for (int i = 0; i < c_log->variable_count; i++) {
    // printf("address is %p\n", c_log->c_data[i].address);
    for (int j = 0; j < num_data; j++) {
      if (addresses[j] == c_log->c_data[i].address) {
        if (c_log->c_data[i].size[0] == 4)
          printf("current value is %d\n", *((int *)pmem_addresses[j]));
        else
          printf("current value is %f\n", *((double *)pmem_addresses[j]));
        // printf("current value is %f or %d\n", *((double
        // *)pmem_addresses[j]),*((int *)pmem_addresses[j]));
        c_data_indices[j] = i;
      }
    }
  }

  // Actual reversion
  int ind = -1;
  for (int i = 0; i < num_data; i++) {
    size_t size = c_log->c_data[c_data_indices[i]].size[version_num];
    ind = search_for_address(addresses[i], size, c_log);
    // printf("ind is %d for %p\n", ind, addresses[i]);
    revert_by_address(addresses[i], pmem_addresses[i], ind, version_num, 0,
                      size, c_log);
    if (size == 4)
      printf("AFTER REVERSION value is %d\n", *((int *)pmem_addresses[i]));
    else
      printf("AFTER REVERSION value is %f\n", *((double *)pmem_addresses[i]));
    // printf("AFTER REVERSION value is %f or %d\n", *((double
    // *)pmem_addresses[i]),
    //    *((int *)pmem_addresses[i]));
  }
  coarse_grained_tries++;
}

PMEMobjpool *redo_pmem_addresses(const char *path, const char *layout,
                                 int num_data, void **pmem_addresses,
                                 uint64_t *offsets)
{
  PMEMobjpool *pop = pmemobj_open(path, layout);
  if (pop == NULL) {
    printf("could not open pop\n");
    return NULL;
  }
  for (int i = 0; i < num_data; i++) {
    pmem_addresses[i] = (void *)((uint64_t)pop + offsets[i]);
  }
  return pop;
}

void re_execute(const char *reexecution_cmd, int version_num, void **addresses,
                struct checkpoint_log *c_log, void **pmem_addresses,
                int num_data, const char *path, const char *layout,
                uint64_t *offsets) {
  int ret_val;
  int reexecute_flag = 0;
  // the reexcution command is a single line command string
  // if multiple commands are needed, they can be specified
  // with 'cmd1 && cmd2 && cmd3' just like how multi-commands are
  // executed in bash. if the rexecution command is so complex,
  // it can also be put into a script and then the rexecution
  // command is simply './rx_script.sh'
  ret_val = system(reexecution_cmd);
  // printf( "********************\n");
  // printf("ret val is %d reexecute is %d\n", ret_val, reexecute_flag);
  if (WIFEXITED(ret_val)) {
    printf("WEXITSTATUS OS %d\n", WEXITSTATUS(ret_val));
    if (WEXITSTATUS(ret_val) < 0 || WEXITSTATUS(ret_val) > 1) {
      reexecute_flag = 1;
    }
  }
  if (coarse_grained_tries == MAX_COARSE_ATTEMPTS) {
    return;
  }
  // Try again if we need to re-execute
  if (reexecute_flag) {
    printf("Reversion attempt %d\n", coarse_grained_tries + 1);
    printf("\n");
    PMEMobjpool *pop =
        redo_pmem_addresses(path, layout, num_data, pmem_addresses, offsets);
    coarse_grain_reversion(addresses, c_log, pmem_addresses, version_num - 1,
                           num_data);
    pmemobj_close(pop);
    printf("Reexecution %d: \n", coarse_grained_tries);
    printf("\n");
    re_execute(reexecution_cmd, version_num - 1, addresses, c_log,
               pmem_addresses, num_data, path, layout, offsets);
  }
}
