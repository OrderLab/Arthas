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

int reverse_cmpfunc(const void *a, const void *b) {
  return (*(int *)b - *(int *)a);
}

void decision_func_sequence_array(int *old_seq_numbers, int old_total,
                                  int *new_seq_numbers, int *new_total) {
  for (int i = 0; i < old_total; i++) {
    new_seq_numbers[i] = old_seq_numbers[i];
    *new_total = *new_total + 1;
  }
  qsort(new_seq_numbers, *new_total, sizeof(int), reverse_cmpfunc);
}

void revert_by_sequence_number_array(seq_log *s_log, int *seq_numbers,
                                     int total_seq_num,
                                     struct checkpoint_log *c_log) {
  int curr_version, rollback_version;
  for (int i = 0; i < total_seq_num; i++) {
    single_data search_data = lookup(s_log, seq_numbers[i]);
    if (search_data.sequence_number == -1) {
      continue;
    }
    curr_version = search_data.version;
    rollback_version = curr_version - 1;
    if (rollback_version < 0) {
      if (search_data.old_checkpoint_entry) {
        struct node *c_node =
            search_for_offset(search_data.old_checkpoint_entry, c_log);
        if (c_node) {
          checkpoint_data old_check_data = c_node->c_data;
          curr_version = old_check_data.version;
          rollback_version = curr_version;
          revert_by_sequence_number_checkpoint(old_check_data, rollback_version,
                                               search_data);
        }
      }
      continue;
    }
    revert_by_sequence_number(search_data, seq_numbers[i], 
                              rollback_version, s_log);
  }
}

void revert_by_transaction(void **sorted_pmem_addresses, struct tx_log *t_log,
                           int *seq_numbers, int total_seq_num,
                           seq_log *s_log) {
  int tx_id;
  int already_reverted = 0;
  for (int i = 0; i < total_seq_num; i++) {
    single_data search_data = lookup(s_log, seq_numbers[i]);
    tx_id = search_data.tx_id;
    int pos = txhashCode(t_log, tx_id);
    struct tx_node *t_list = t_log->list[pos];
    struct tx_node *t_temp = t_list;
    while (t_temp) {
      for (int j = 0; j < total_seq_num; j++) {
        if (t_temp->ordered_data.sequence_number == seq_numbers[j]) {
          already_reverted = 1;
        }
      }
      if (t_temp->ordered_data.version != 0 && !already_reverted) {
        search_data = lookup(s_log, t_temp->ordered_data.sequence_number);
        revert_by_sequence_number(search_data,
                                  t_temp->ordered_data.sequence_number,
                                  t_temp->ordered_data.version - 1, s_log);
      }
      already_reverted = 0;
      t_temp = t_temp->next;
    }
  }
}

void revert_by_sequence_number_nonslice(void *old_pop, single_data ordered_data,
                                        int seq_num, int rollback_version,
                                        void *pop) {
  if (rollback_version < 0) return;
  void *pmem_address = (void *)((uint64_t)ordered_data.address -
                                (uint64_t)old_pop + (uint64_t)pop);
  memcpy(pmem_address, ordered_data.old_data[rollback_version],
         ordered_data.old_size[rollback_version]);
}

void undo_by_sequence_number(single_data search_data, int seq_num) {
  int curr_version = search_data.version;
  int rollback_version = curr_version - 1;
  if (rollback_version < 0) {
    return;
  }
  memcpy(search_data.sorted_pmem_address, search_data.data, search_data.size);
}

void revert_by_sequence_number_checkpoint(checkpoint_data old_check_data,
                                          int rollback_version,
                                          single_data search_data) {
  memcpy(search_data.sorted_pmem_address, old_check_data.data[rollback_version],
         old_check_data.size[rollback_version]);
}

void revert_by_sequence_number(single_data search_data, int seq_num,
                               int rollback_version, seq_log *s_log) {
  lookup_undo_save(s_log, seq_num, search_data.sorted_pmem_address,
                   search_data.size);
  memcpy(search_data.sorted_pmem_address,
         search_data.old_data[rollback_version],
         search_data.old_size[rollback_version]);
}

void sort_by_sequence_number(void **addresses, size_t total_size, int num_data,
                             void **pmem_addresses,
                             void **sorted_pmem_addresses, uint64_t *offsets,
                             seq_log *s_log) {
  struct seq_node *list;
  struct seq_node *temp;
  for (int i = 0; i < (int)s_log->size; i++) {
    for (int j = 0; j < num_data; j++) {
      list = s_log->list[i];
      temp = list;
      while (temp) {
        if (temp->ordered_data.offset == offsets[j]) {
          temp->ordered_data.sorted_pmem_address = pmem_addresses[j];
        }
        temp = temp->next;
      }
    }
  }
}


void seq_coarse_grain_reversion(uint64_t *offsets, void **sorted_pmem_addresses,
                                int seq_num, void *pop, void *old_pop,
                                struct checkpoint_log *c_log, seq_log *s_log) {
  single_data revert_data = lookup(s_log, seq_num);
  int curr_version = revert_data.version;
  printf("curr version is %d\n", curr_version);
  int rollback_version = curr_version - 1;
  if (rollback_version < 0) {
    return;
  }
  revert_by_sequence_number_nonslice(old_pop, revert_data, seq_num,
                                     curr_version - 1, pop);
}

int checkpoint_hashcode(checkpoint_log *c_log, uint64_t offset) {
  int ret_val;
  if (offset < 0) {
    ret_val = (int)(offset % c_log->size);
    ret_val = -ret_val;
  }
  ret_val = (int)(offset % c_log->size);
  return ret_val;
}

struct node *search_for_offset(uint64_t old_off, checkpoint_log *c_log) {
  int pos = checkpoint_hashcode(c_log, old_off);
  struct node *list = c_log->list[pos];
  struct node *temp = list;
  while (temp) {
    if (temp->offset == old_off) {
      return temp;
    }
    temp = temp->next;
  }
  return NULL;
}

PMEMobjpool *redo_pmem_addresses(const char *path, const char *layout,
                                 int num_data, uint64_t *offsets,
                                 seq_log *s_log) {
  PMEMobjpool *pop = pmemobj_open(path, layout);
  printf("new pop is %p\n", pop);
  if (pop == NULL) {
    printf("could not open pop %s\n", pmemobj_errormsg());
    return NULL;
  }
  struct seq_node *list;
  struct seq_node *temp;
  for (int i = 0; i < (int)s_log->size; i++) {
    list = s_log->list[i];
    temp = list;
    while (temp) {
      temp->ordered_data.sorted_pmem_address =
          (void *)((uint64_t)pop + temp->ordered_data.offset);
      temp = temp->next;
    }
  }
  return pop;
}

int re_execute(const char *reexecution_cmd, int version_num,
               struct checkpoint_log *c_log, void **pmem_addresses,
               int num_data, const char *path, const char *layout,
               uint64_t *offsets, int reversion_type, int seq_num,
               void **sorted_pmem_addresses, void *old_pop, 
               seq_log *s_log) {
  int ret_val;
  int reexecute_flag = 0;
  // the reexcution command is a single line command string
  // if multiple commands are needed, they can be specified
  // with 'cmd1 && cmd2 && cmd3' just like how multi-commands are
  // executed in bash. if the rexecution command is so complex,
  // it can also be put into a script and then the rexecution
  // command is simply './rx_script.sh'
  // ret_val = system(timeout);
  ret_val = system(reexecution_cmd);
  printf("ret val is %d reexecute is %d\n", ret_val, reexecute_flag);
  if (WIFEXITED(ret_val)) {
    printf("WEXITSTATUS OS %d\n", WEXITSTATUS(ret_val));
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

    PMEMobjpool *pop = redo_pmem_addresses(path, layout, num_data,
                                           offsets, s_log);
    if (!pop) {
      system("./killScript");
      pop = redo_pmem_addresses(path, layout, num_data, offsets,
                                s_log);
    }
    if (reversion_type == COARSE_GRAIN_NAIVE) {
      // coarse_grain_reversion(addresses, c_log, pmem_addresses, version_num -
      // 1,
      //                       num_data, offsets);
    } else if (reversion_type == COARSE_GRAIN_SEQUENCE) {
      if (seq_num < 0) return -1;
      seq_coarse_grain_reversion(offsets, sorted_pmem_addresses, seq_num,
                                 pop, old_pop, c_log, s_log);
    } else {
      pmemobj_close(pop);
      return -1;
    }
    pmemobj_close(pop);
    printf("Reexecution %d: \n", coarse_grained_tries);
    printf("\n");
    re_execute(reexecution_cmd, version_num - 1, c_log,
               pmem_addresses, num_data, path, layout, offsets, reversion_type,
               seq_num - 1, sorted_pmem_addresses, old_pop,
               s_log);
  }
  return 1;
}
