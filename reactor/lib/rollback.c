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

/*int search_for_offset(uint64_t offset, size_t size,
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
  }
  return -1;
} */

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
                                     seq_log *s_log, int *seq_numbers,
                                     int total_seq_num,
                                     struct checkpoint_log *c_log) {
  int curr_version, rollback_version;
  for (int i = 0; i < total_seq_num; i++) {
    single_data search_data = lookup(s_log, seq_numbers[i]);
    // printf("seq num is %d\n", seq_numbers[i]);
    if (search_data.sequence_number == -1) {
      continue;
    }
    curr_version = search_data.version;
    // curr_version = ordered_data[seq_numbers[i]].version;
    rollback_version = curr_version - 1;
    if (rollback_version < 0) {
      // printf("checkpoint_realloc\n");
      // TODO: right now, assumes insertion is done in reverse order
      // only one indirection chain done here rather than a loop.
      // printf("skipping %d\n", seq_numbers[i]);
      // printf("looking for realloc old version of seq num %d\n",
      // seq_numbers[i]);
      if (search_data.old_checkpoint_entry) {
        struct node *c_node =
            search_for_offset(search_data.old_checkpoint_entry, c_log);
        if (c_node) {
          // printf("found old checkpoint entry\n");
          checkpoint_data old_check_data = c_node->c_data;
          curr_version = old_check_data.version;
          rollback_version = curr_version;
          revert_by_sequence_number_checkpoint(sorted_pmem_addresses,
                                               old_check_data, rollback_version,
                                               search_data);
        }
      }
      /*int old_index =
          search_for_offset(ordered_data[seq_numbers[i]].old_checkpoint_entry,
                            ordered_data[seq_numbers[i]].size, c_log);
      if (old_index < 0) continue;
      curr_version = c_log->c_data[old_index].version;
      rollback_version = curr_version - 1;
      // revert_by_offset(ordered_data[seq_numbers[i]].old_checkpoint_entry,
      //                 c_log->c_data[old_index].address, old_index,
      //                 rollback_version,
      //                  0, c_log->c_data[old_index].size[rollback_version],
      //                  c_log);
      revert_by_offset(ordered_data[seq_numbers[i]].old_checkpoint_entry,
                       ordered_data[seq_numbers[i]].address, old_index,
                       rollback_version, 0,
                       c_log->c_data[old_index].size[rollback_version],
      c_log);*/
      continue;
    }
    // printf("rollback version is %d\n", rollback_version);
    revert_by_sequence_number(sorted_pmem_addresses, search_data,
                              seq_numbers[i], rollback_version, s_log);
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
      // printf("data is %f or %d tx id is %d\n", *((double
      // *)t_temp->ordered_data.data$
      //     *((int *)t_temp->ordered_data.data), t_temp->ordered_data.tx_id);
      printf("revert tx id of %d seq num is %d\n", tx_id,
             t_temp->ordered_data.sequence_number);
      for (int j = 0; j < total_seq_num; j++) {
        if (t_temp->ordered_data.sequence_number == seq_numbers[j]) {
          already_reverted = 1;
        }
      }
      if (t_temp->ordered_data.version != 0 && !already_reverted) {
        search_data = lookup(s_log, t_temp->ordered_data.sequence_number);
        revert_by_sequence_number(sorted_pmem_addresses, search_data,
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
  printf("seq num is %d, rollback version is %d, size is %ld", seq_num,
         rollback_version, ordered_data.old_size[rollback_version]);
  void *pmem_address = (void *)((uint64_t)ordered_data.address -
                                (uint64_t)old_pop + (uint64_t)pop);
  // void *pmem_address = ordered_data.sorted_pmem_address;
  printf("pmem_address is %p old_pop is %p\n", pmem_address, old_pop);
  // printf("nbytes is %d\n", *((int
  // *)((uint64_t)ordered_data[seq_num].old_data[rollback_version] + 12 +
  // sizeof(unsigned int) *2)) );
  // if (ordered_data[seq_num].old_size[rollback_version] == 64)
  memcpy(pmem_address, ordered_data.old_data[rollback_version],
         ordered_data.old_size[rollback_version]);
}

void undo_by_sequence_number(single_data search_data, int seq_num) {
  /*if(search_data.size == 4){
    printf("Value before seq num %d is %d offset %ld size is %ld\n", seq_num,
           *(int *)search_data.sorted_pmem_address,
           search_data.offset, search_data.size);
  }
  else if(search_data.size == 8){
    printf("Value before seq num %d is %f offset %ld size is %ld\n", seq_num,
           *(double *)search_data.sorted_pmem_address,
           search_data.offset, search_data.size);
  }*/
  // if(search_data.size != 48)
  //	printf("IN UNDO SIZE IS NOT 48 FOR SOME REASON!!!!!\n");
  // printf("seq num is %d %p undo size %d %d\n", seq_num,
  // search_data.sorted_pmem_address, search_data.size, sizeof (item));
  // printf("search_data address is %p\n", search_data.address);
  // printf("before undo %d %f %s\n", *(int *)search_data.sorted_pmem_address,
  // *(double *)search_data.sorted_pmem_address, (char
  // *)search_data.sorted_pmem_address);
  // printf("before undo %d %f %s\n", *(int *)search_data.data,
  // *(double *)search_data.data, (char *)search_data.data);
  //  item *ipmem = (item *)search_data.sorted_pmem_address;
  //  item *idata = (item *)search_data.data;
  // printf("ipmem is %d %d %s %s\n", ipmem->nkey, ipmem->nbytes, ipmem->data,
  // ITEM_key(ipmem));
  // printf("idata is %d %d %s %s\n", idata->nkey, idata->nbytes, idata->data,
  // ITEM_key(idata));
  int curr_version = search_data.version;
  int rollback_version = curr_version - 1;
  if (rollback_version < 0) {
    //    printf("oldest version\n");
    return;
  }
  memcpy(search_data.sorted_pmem_address, search_data.data, search_data.size);
  // printf("after undo %d %f %s\n", *(int *)search_data.sorted_pmem_address,
  // *(double *)search_data.sorted_pmem_address, (char
  // *)search_data.sorted_pmem_address);
  // ipmem = (item *)search_data.sorted_pmem_address;
  // printf("ipmem is %d %d %s %s\n", ipmem->nkey, ipmem->nbytes, ipmem->data,
  // ITEM_key(ipmem));
  /*if(search_data.size == 4){
    printf("Value after seq num %d is %d offset %ld size is %ld\n", seq_num,
           *(int *)search_data.sorted_pmem_address,
           search_data.offset, search_data.size);
  }
  else if(search_data.size == 8){
    printf("Value after seq num %d is %f offset %ld size is %ld\n", seq_num,
           *(double *)search_data.sorted_pmem_address,
           search_data.offset, search_data.size);
  }*/
}

void revert_by_sequence_number_checkpoint(void **sorted_pmem_addresses,
                                          checkpoint_data old_check_data,
                                          int rollback_version,
                                          single_data search_data) {
  memcpy(search_data.sorted_pmem_address, old_check_data.data[rollback_version],
         old_check_data.size[rollback_version]);
}

void revert_by_sequence_number(void **sorted_pmem_addresses,
                               single_data search_data, int seq_num,
                               int rollback_version, seq_log *s_log) {
  // printf("%p \n", sorted_pmem_addresses[seq_num]);
  // printf("value here is %d \n", *(int
  // *)ordered_data[seq_num].old_data[rollback_version]);
  // printf("value here is %d \n", *(int *)sorted_pmem_addresses[seq_num]);
  // printf("%p \n", ordered_data[seq_num].old_data[rollback_version]);
  // printf("%ld \n", ordered_data[seq_num].old_size[rollback_version]);
  /*printf("seq num is %d pmem_is_pmem %d size is %ld\n", seq_num,
         pmem_is_pmem(sorted_pmem_addresses[seq_num],
                      ordered_data[seq_num].old_size[rollback_version]),
         ordered_data[seq_num].old_size[rollback_version]);*/
  /*if (search_data.old_size[rollback_version] == 4)
    printf("Value before seq num %d is %d offset %ld size is %ld\n", seq_num,
           *(int *)search_data.sorted_pmem_address,
           search_data.offset, search_data.old_size[rollback_version]);
  else if (search_data.old_size[rollback_version] == 8)
    printf("Value before seq num %d is %f offset %ld size is %ld\n", seq_num,
           *(double *)search_data.sorted_pmem_address,
           search_data.offset, search_data.old_size[rollback_version]);
  else
    printf("Value before seq num %d is %d or %s offset %ld size is %ld\n",
  seq_num,
           *(int *)search_data.sorted_pmem_address,
           (char *)search_data.sorted_pmem_address,
           search_data.offset,
           search_data.old_size[rollback_version]);*/
  /*printf("revert address %p %ld\n", sorted_pmem_addresses[seq_num],
         (uint64_t)sorted_pmem_addresses[seq_num]);*/

  // printf("seq num is %d %p %d\n", seq_num, search_data.sorted_pmem_address,
  // search_data.size);
  // printf("search_data address is %p\n", search_data.address);
  // printf("before reversion %d %f %s\n", *(int
  // *)search_data.sorted_pmem_address,
  // *(double *)search_data.sorted_pmem_address, (char
  // *)search_data.sorted_pmem_address);
  // printf("before reversion %d %f %s\n", *(int *)search_data.data,
  // *(double *)search_data.data, (char *)search_data.data);

  /*if(search_data.size == 48){
  item *ipmem = (item *)search_data.sorted_pmem_address;
  item *idata = (item *)search_data.data;
  printf("ipmem is %d %d %p %s\n", ipmem->nkey, ipmem->nbytes, ipmem->next,
  ITEM_key(ipmem));
  printf("idata is %d %d %p %s\n", idata->nkey, idata->nbytes, idata->next,
  ITEM_key(idata));
  //printf("pmem is pmem %d", pmem_is_pmem(search_data.sorted_pmem_address,
  search_data.old_size[rollback_version]));
  }*/
  lookup_undo_save(s_log, seq_num, search_data.sorted_pmem_address,
                   search_data.size);
  memcpy(search_data.sorted_pmem_address,
         search_data.old_data[rollback_version],
         search_data.old_size[rollback_version]);
  // printf("after reversion %d %f %s\n", *(int
  // *)search_data.sorted_pmem_address,
  //  *(double *)search_data.sorted_pmem_address, (char
  //  *)search_data.sorted_pmem_address);
  /*if(search_data.size == 48){
   item *ipmem = (item *)search_data.sorted_pmem_address;
   printf("ipmem is %d %d %s %s\n", ipmem->nkey, ipmem->nbytes, ipmem->next,
   ITEM_key(ipmem));
   }*/
  // printf("value here is %d \n", *(int
  // *)ordered_data[seq_num].old_data[rollback_version]);
  /*if (search_data.old_size[rollback_version] == 4)
    printf("Value after seq num %d is %d offset %ld\n", seq_num,
           *(int *)search_data.sorted_pmem_address,
           search_data.offset);
  else if (search_data.old_size[rollback_version] == 8)
    printf("Value after seq num %d is %f offset %ld\n", seq_num,
           *(double *)search_data.sorted_pmem_address,
           search_data.offset);
  else
    printf("Value after seq num %d is %d or %s offset %ld\n", seq_num,
           *(int *)search_data.sorted_pmem_address,
           (char *)search_data.sorted_pmem_address,
           search_data.offset);*/

  /*if (ordered_data[seq_num].old_size[rollback_version] == 4)
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
           ordered_data[seq_num].offset);*/
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
  /*for (int i = 0; i < total_size; i++) {
    for (int j = 0; j < num_data; j++) {
      // printf("ordered data offset is %ld, offset is %ld\n",
      // ordered_data[i].offset, offsets[j]);
      if (ordered_data[i].offset == offsets[j]) {
        sorted_addresses[i] = ordered_data[i].address;
        sorted_pmem_addresses[i] = pmem_addresses[j];
      }
    }
  }*/
}

/*void revert_by_address(const void *search_address, const void *address,
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
    printf("offset reversion successful\n");
    memcpy(dest, c_log->c_data[variable_index].data[version],
           c_log->c_data[variable_index].size[version]);
  } else {
    // memcpy(dest, (void *)((uint64_t)pmem_file + search_offset), size);
  }
}*/

void seq_coarse_grain_reversion(uint64_t *offsets, void **sorted_pmem_addresses,
                                int seq_num, single_data *ordered_data,
                                void *pop, void *old_pop,
                                struct checkpoint_log *c_log, seq_log *s_log) {
  single_data revert_data = lookup(s_log, seq_num);
  int curr_version = revert_data.version;
  // int curr_version = ordered_data[seq_num].version;
  printf("curr version is %d\n", curr_version);
  int rollback_version = curr_version - 1;
  if (rollback_version < 0) {
    /*printf("look for old checkpoint entry %ld\n",
           ordered_data[seq_num].old_checkpoint_entry);
    int old_index =
        search_for_offset(ordered_data[seq_num].old_checkpoint_entry,
                          ordered_data[seq_num].size, c_log);
    if (old_index < 0) {
      printf("old index ended up being not found\n");
      return;
    }
    curr_version = c_log->c_data[old_index].version;
    rollback_version = curr_version - 1;
    revert_by_offset(ordered_data[seq_num].old_checkpoint_entry,
                     ordered_data[seq_num].address, old_index, rollback_version,
                     0, c_log->c_data[old_index].size[rollback_version],
    c_log);*/
    return;
  }
  revert_by_sequence_number_nonslice(old_pop, revert_data, seq_num,
                                     curr_version - 1, pop);
}

/*void coarse_grain_reversion(void **addresses, struct checkpoint_log *c_log,
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
}*/

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
                                 int num_data, void **pmem_addresses,
                                 uint64_t *offsets, seq_log *s_log) {
  PMEMobjpool *pop = pmemobj_open(path, layout);
  printf("new pop is %p\n", pop);
  if (pop == NULL) {
    printf("could not open pop %s\n", pmemobj_errormsg());
    return NULL;
  }
  // TODO: redo pmem_addresses of search_data...
  /*for (int i = 0; i < num_data; i++) {
    pmem_addresses[i] = (void *)((uint64_t)pop + offsets[i]);
  }*/
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

int re_execute(const char *reexecution_cmd, int version_num, void **addresses,
               struct checkpoint_log *c_log, void **pmem_addresses,
               int num_data, const char *path, const char *layout,
               uint64_t *offsets, int reversion_type, int seq_num,
               void **sorted_pmem_addresses, single_data *ordered_data,
               void *old_pop, seq_log *s_log) {
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
  // printf( "********************\n");
  printf("ret val is %d reexecute is %d\n", ret_val, reexecute_flag);
  // ret_val = -1;
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

    PMEMobjpool *pop = redo_pmem_addresses(path, layout, num_data,
                                           pmem_addresses, offsets, s_log);
    if (!pop) {
      system("./killScript");
      pop = redo_pmem_addresses(path, layout, num_data, pmem_addresses, offsets,
                                s_log);
    }
    if (reversion_type == COARSE_GRAIN_NAIVE) {
      // coarse_grain_reversion(addresses, c_log, pmem_addresses, version_num -
      // 1,
      //                       num_data, offsets);
    } else if (reversion_type == COARSE_GRAIN_SEQUENCE) {
      if (seq_num < 0) return -1;
      seq_coarse_grain_reversion(offsets, sorted_pmem_addresses, seq_num,
                                 ordered_data, pop, old_pop, c_log, s_log);
    } else {
      pmemobj_close(pop);
      return -1;
    }
    pmemobj_close(pop);
    printf("Reexecution %d: \n", coarse_grained_tries);
    printf("\n");
    re_execute(reexecution_cmd, version_num - 1, addresses, c_log,
               pmem_addresses, num_data, path, layout, offsets, reversion_type,
               seq_num - 1, sorted_pmem_addresses, ordered_data, old_pop,
               s_log);
  }
  return 1;
}
