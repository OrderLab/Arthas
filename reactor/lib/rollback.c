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

int search_for_offset(uint64_t offset, size_t size,
                      struct checkpoint_log *c_log) {
  uint64_t search_upper_bound = offset + (uint64_t)size;
  uint64_t clog_upper_bound;
  for (int i = 0; i < c_log->variable_count; i++) {
    uint64_t clog_offset = c_log->c_data[i].offset;
    clog_upper_bound =
        (uint64_t)clog_offset + (uint64_t)c_log->c_data[i].size[0];
    if (offset >= clog_offset && search_upper_bound <= clog_upper_bound) {
      return i;
    }
  }
  return -1;
}

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

int reverse_cmpfunc(const void *a, const void *b) {
  return (*(int *)b - *(int *)a);
}

void decision_func_sequence_array(int *old_seq_numbers, int old_total,
                                  int *new_seq_numbers, int *new_total) {
  // TODO: Right now decision function is just based on sequence number
  // ordering
  for (int i = 0; i < old_total; i++) {
    new_seq_numbers[i] = old_seq_numbers[i];
    *new_total = *new_total + 1;
  }
  qsort(new_seq_numbers, *new_total, sizeof(int), reverse_cmpfunc);
}

void revert_by_sequence_number_array(void **sorted_pmem_addresses,
                                     single_data *ordered_data,
                                     int *seq_numbers, int total_seq_num) {
  int curr_version, rollback_version;
  for (int i = 0; i < total_seq_num; i++) {
    curr_version = ordered_data[seq_numbers[i]].version;
    rollback_version = curr_version - 1;
    if (rollback_version < 0) {
      continue;
    }
    // printf("rollback version is %d\n", rollback_version);
    revert_by_sequence_number(sorted_pmem_addresses, ordered_data,
                              seq_numbers[i], rollback_version);
  }
}

void revert_by_sequence_number_nonslice(void *old_pop,
                                        single_data *ordered_data, int seq_num,
                                        int rollback_version, void *pop) {
  void *pmem_address = (void *)((uint64_t)ordered_data[seq_num].address -
                                (uint64_t)old_pop + (uint64_t)pop);
  printf("seq num is %d, pmem_is_pmem %d size is %ld", seq_num,
         pmem_is_pmem(pmem_address,
                      ordered_data[seq_num].old_size[rollback_version]),
         ordered_data[seq_num].old_size[rollback_version]);
  printf("pmem_address is %p old_pop is %p\n", pmem_address, old_pop);
  /*if(pmem_is_pmem(pmem_address,
  ordered_data[seq_num].old_size[rollback_version]) == 0){
    printf("not pmem, reversion abortion \n");
    return;
  }*/
  //if (ordered_data[seq_num].old_size[rollback_version] == 64)
    memcpy(pmem_address, ordered_data[seq_num].old_data[rollback_version],
           ordered_data[seq_num].old_size[rollback_version]);
}

void revert_by_sequence_number(void **sorted_pmem_addresses,
                               single_data *ordered_data, int seq_num,
                               int rollback_version) {
  // printf("%p \n", sorted_pmem_addresses[seq_num]);
  // printf("value here is %d \n", *(int
  // *)ordered_data[seq_num].old_data[rollback_version]);
  // printf("value here is %d \n", *(int *)sorted_pmem_addresses[seq_num]);
  // printf("%p \n", ordered_data[seq_num].old_data[rollback_version]);
  // printf("%ld \n", ordered_data[seq_num].old_size[rollback_version]);
  printf("seq num is %d pmem_is_pmem %d size is %ld\n", seq_num,
         pmem_is_pmem(sorted_pmem_addresses[seq_num],
                      ordered_data[seq_num].old_size[rollback_version]),
         ordered_data[seq_num].old_size[rollback_version]);
  if (pmem_is_pmem(sorted_pmem_addresses[seq_num],
                   ordered_data[seq_num].old_size[rollback_version]) == 0) {
    printf("this is not pmem \n");
    return;
  }
  if (ordered_data[seq_num].old_size[rollback_version] == 4)
    printf("Value before seq num %d is %d offset %ld\n", seq_num,
           *(int *)sorted_pmem_addresses[seq_num],
           ordered_data[seq_num].offset);
  else if (ordered_data[seq_num].old_size[rollback_version] == 8)
    printf("Value before seq num %d is %f offset %ld\n", seq_num,
           *(double *)sorted_pmem_addresses[seq_num],
           ordered_data[seq_num].offset);
  else
    printf("Value before seq num %d is %d or %s offset %ld\n", seq_num,
           *(int *)sorted_pmem_addresses[seq_num],
           (char *)sorted_pmem_addresses[seq_num],
           ordered_data[seq_num].offset);
  printf("revert address %p %ld\n", sorted_pmem_addresses[seq_num],
         (uint64_t)sorted_pmem_addresses[seq_num]);
  memcpy(sorted_pmem_addresses[seq_num],
         ordered_data[seq_num].old_data[rollback_version],
         ordered_data[seq_num].old_size[rollback_version]);
  // printf("value here is %d \n", *(int
  // *)ordered_data[seq_num].old_data[rollback_version]);
  if (ordered_data[seq_num].old_size[rollback_version] == 4)
    printf("REVERTED Value before seq num %d is %d offset %ld\n", seq_num,
           *(int *)sorted_pmem_addresses[seq_num],
           ordered_data[seq_num].offset);
  else if (ordered_data[seq_num].old_size[rollback_version] == 8)
    printf("REVERTED Value before seq num %d is %f offset %ld\n", seq_num,
           *(double *)sorted_pmem_addresses[seq_num],
           ordered_data[seq_num].offset);
  else
    printf("REVERTED Value before seq num %d is %d or %s offset %ld\n", seq_num,
           *(int *)sorted_pmem_addresses[seq_num],
           (char *)sorted_pmem_addresses[seq_num],
           ordered_data[seq_num].offset);
}

void sort_by_sequence_number(void **addresses, single_data *ordered_data,
                             size_t total_size, int num_data,
                             void **sorted_addresses, void **pmem_addresses,
                             void **sorted_pmem_addresses, uint64_t *offsets) {
  for (int i = 0; i < total_size; i++) {
    for (int j = 0; j < num_data; j++) {
      // printf("ordered data offset is %ld, offset is %ld\n",
      // ordered_data[i].offset, offsets[j]);
      if (ordered_data[i].offset == offsets[j]) {
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

void revert_by_offset(uint64_t search_offset, const void *address,
                      int variable_index, int version, int type, size_t size,
                      struct checkpoint_log *c_log) {
  void *dest = (void *)address;
  if (search_offset == c_log->c_data[variable_index].offset) {
    memcpy(dest, c_log->c_data[variable_index].data[version],
           c_log->c_data[variable_index].size[version]);
  } else {
    // memcpy(dest, (void *)((uint64_t)pmem_file + search_offset), size);
  }
}

void seq_coarse_grain_reversion(uint64_t *offsets, void **sorted_pmem_addresses,
                                int seq_num, single_data *ordered_data,
                                void *pop, void *old_pop) {
  int curr_version = ordered_data[seq_num].version;
  revert_by_sequence_number_nonslice(old_pop, ordered_data, seq_num,
                                     curr_version - 1, pop);
}

void coarse_grain_reversion(void **addresses, struct checkpoint_log *c_log,
                            void **pmem_addresses, int version_num,
                            int num_data, uint64_t *offsets) {
  int c_data_indices[MAX_DATA];
  for (int i = 0; i < c_log->variable_count; i++) {
    // printf("address is %p num_data is %d\n", c_log->c_data[i].address,
    // num_data);
    printf("offset is %ld\n", c_log->c_data[i].offset);
    for (int j = 0; j < num_data; j++) {
      // printf("addresses is %p\n", addresses[j]);
      if (offsets[j] == c_log->c_data[i].offset) {
        // if (addresses[j] == c_log->c_data[i].address) {
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
  for (int i = 0; i < c_log->variable_count; i++) {
    size_t size = c_log->c_data[c_data_indices[i]].size[version_num];
    // ind = search_for_address(addresses[i], size, c_log)
    size = c_log->c_data[i].size[version_num];
    // printf("ind is %d for %p\n", ind, addresses[i]);
    for (int j = 0; j < num_data; j++) {
      if (offsets[j] == c_log->c_data[i].offset) {
        ind = search_for_offset(offsets[j], size, c_log);
        revert_by_offset(offsets[j], pmem_addresses[j], ind, version_num, 0,
                         size, c_log);
        if (size == 4)
          printf("AFTER REVERSION value is %d\n", *((int *)pmem_addresses[j]));
        else
          printf("AFTER REVERSION value is %f\n",
                 *((double *)pmem_addresses[j]));
      }
    }
    // revert_by_address(addresses[i], pmem_addresses[i], ind, version_num, 0,
    //                  size, c_log);
    // printf("AFTER REVERSION value is %f or %d\n", *((double
    // *)pmem_addresses[i]),
    //    *((int *)pmem_addresses[i]));
  }
  coarse_grained_tries++;
}

PMEMobjpool *redo_pmem_addresses(const char *path, const char *layout,
                                 int num_data, void **pmem_addresses,
                                 uint64_t *offsets) {
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

int re_execute(const char *reexecution_cmd, int version_num, void **addresses,
               struct checkpoint_log *c_log, void **pmem_addresses,
               int num_data, const char *path, const char *layout,
               uint64_t *offsets, int reversion_type, int seq_num,
               void **sorted_pmem_addresses, single_data *ordered_data,
               void *old_pop) {
  int ret_val;
  int reexecute_flag = 0;
  // char timeout[1000] = "timeout 15 ";
  // strcat(timeout, reexecution_cmd);
  // the reexcution command is a single line command string
  // if multiple commands are needed, they can be specified
  // with 'cmd1 && cmd2 && cmd3' just like how multi-commands are
  // executed in bash. if the rexecution command is so complex,
  // it can also be put into a script and then the rexecution
  // command is simply './rx_script.sh'
  // ret_val = system(timeout);
  ret_val = system(reexecution_cmd);
  // printf( "********************\n");
  // printf("ret val is %d reexecute is %d\n", ret_val, reexecute_flag);
  if (WIFEXITED(ret_val)) {
    printf("WEXITSTATUS OS %d\n", WEXITSTATUS(ret_val));
    // if (WEXITSTATUS(ret_val) < 0 || WEXITSTATUS(ret_val) > 1) {
    if (WEXITSTATUS(ret_val) != 0) {
      reexecute_flag = 1;
    }
  }
  if (coarse_grained_tries == MAX_COARSE_ATTEMPTS) {
    return -1;
  }
  // Try again if we need to re-execute
  if (reexecute_flag) {
    printf("Reversion attempt %d\n", coarse_grained_tries + 1);
    printf("\n");
    // TODO: add libpmem support
    PMEMobjpool *pop =
        redo_pmem_addresses(path, layout, num_data, pmem_addresses, offsets);
    if (reversion_type == COARSE_GRAIN_NAIVE)
      coarse_grain_reversion(addresses, c_log, pmem_addresses, version_num - 1,
                             num_data, offsets);
    else if (reversion_type == COARSE_GRAIN_SEQUENCE) {
      if (seq_num < 0) return -1;
      seq_coarse_grain_reversion(offsets, sorted_pmem_addresses, seq_num,
                                 ordered_data, pop, old_pop);
    } else {
      pmemobj_close(pop);
      return -1;
    }
    pmemobj_close(pop);
    printf("Reexecution %d: \n", coarse_grained_tries);
    printf("\n");
    re_execute(reexecution_cmd, version_num - 1, addresses, c_log,
               pmem_addresses, num_data, path, layout, offsets, reversion_type,
               seq_num - 1, sorted_pmem_addresses, ordered_data, old_pop);
  }
  return 1;
}
