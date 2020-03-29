// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "checkpoint.h"

struct checkpoint_log *reconstruct_checkpoint(const char *file_path,
                                              const char *pmem_library) {
  int variable_count;
  struct checkpoint_log *c_log;
  if (strcmp(pmem_library, "libpmemobj") == 0) {
    PMEMobjpool *pop = pmemobj_open(file_path, "checkpoint");
    if (!pop) {
      fprintf(stderr, "pool not found\n");
      pmemobj_errormsg();
      return NULL;
    }
    PMEMoid oid = pmemobj_root(pop, sizeof(uint64_t));
    uint64_t *old_pool = (uint64_t *)pmemobj_direct(oid);

    PMEMoid clog_oid = POBJ_FIRST_TYPE_NUM(pop, 0);
    c_log = (struct checkpoint_log *)pmemobj_direct(clog_oid);

    uint64_t offset;
    offset = (uint64_t)c_log->c_data - *old_pool;
    variable_count = c_log->variable_count;
    for (int i = 0; i < variable_count; i++) {
      // printf("variable %d\n", i);
      int data_index = c_log->c_data[i].version;
      // printf("total versions is %d\n", data_index);
      for (int j = 0; j <= data_index; j++) {
        // printf("version %d\n", j);
        offset = (uint64_t)c_log->c_data[i].data[j] - *old_pool;
        // printf("offset is %ld\n", offset);
        c_log->c_data[i].data[j] = (void *)((uint64_t)pop + offset);
      }
    }
    *old_pool = (uint64_t)pop;
  } else if (strcmp(pmem_library, "libpmem") == 0) {
    // TODO:open memory mapped file in a different manner
    char *pmemaddr;
    size_t mapped_len;
    int is_pmem;

    if ((pmemaddr = (char *)pmem_map_file(file_path, PMEM_LEN, PMEM_FILE_CREATE,
                                          0666, &mapped_len, &is_pmem)) ==
        NULL) {
      perror("pmem_mapping failure\n");
      exit(1);
    }
    c_log = (struct checkpoint_log *)pmemaddr;
    uint64_t *old_pool_ptr =
        (uint64_t *)((uint64_t)c_log + sizeof(struct checkpoint_log));
    uint64_t old_pool = *old_pool_ptr;

    uint64_t offset;
    variable_count = c_log->variable_count;
    // printf("variable_count %d\n", c_log->variable_count);
    // printf("old pool ptr is %ld\n", old_pool);
    // offset = (uint64_t)c_log->c_data[0].data[0] - old_pool;
    for (int i = 0; i < variable_count; i++) {
      // printf("version is %d\n", c_log->c_data[i].version);
      // printf("address is %p\n", c_log->c_data[i].address);
      for (int j = 0; j <= c_log->c_data[i].version; j++) {
        offset = (uint64_t)c_log->c_data[i].data[j] - old_pool;
        // printf("offset is %ld\n", offset);
        // printf("size is %ld\n", c_log->c_data[i].size[j]);
        c_log->c_data[i].data[j] = (void *)((uint64_t)c_log + offset);
        // printf("data is %s\n", (char *)c_log->c_data[i].data[j]);
      }
    }
  }
  if (c_log == NULL) {
    return NULL;
  }
  printf("RECONSTRUCTED CHECKPOINT COMPONENT:\n");
  printf("variable count is %d\n", variable_count);
  for (int i = 0; i < variable_count; i++) {
    printf("address is %p offset is %ld\n", c_log->c_data[i].address,
           c_log->c_data[i].offset);
    // printf("version is %d\n", c_log->c_data[i].version);
    int data_index = c_log->c_data[i].version;
    for (int j = 0; j <= data_index; j++) {
      printf("version is %d ", j);
      printf("size is %ld  ", c_log->c_data[i].size[0]);
      printf("seq num is %d  ", c_log->c_data[i].sequence_number[j]);
      if (c_log->c_data[i].size[0] == 4)
        printf("int value is %d\n", *((int *)c_log->c_data[i].data[j]));
      else if (c_log->c_data[i].size[0] == 8)
        printf("double value is %f\n", *((double *)c_log->c_data[i].data[j]));
      else if (c_log->c_data[i].size[0] == sizeof(unsigned short))
        printf("unsigned short value is %hu\n",
               *((unsigned short *)c_log->c_data[i].data[j]));
      else {
        printf("value is not int or double %s\n",
               (char *)c_log->c_data[i].data[j]);
      }
      // else
      //  printf("value is %s\n", (char *)c_log->c_data[i].data[j]);
      // printf("version is %d, value is %f or %d\n", j, *((double
      // *)c_log->c_data[i].data[j]),*((int *)c_log->c_data[i].data[j]));
    }
  }
  return c_log;
}

int sequence_comparator(const void *v1, const void *v2) {
  single_data *s1 = (single_data *)v1;
  single_data *s2 = (single_data *)v2;
  if (s1->sequence_number < s2->sequence_number)
    return -1;
  else if (s1->sequence_number > s2->sequence_number)
    return 1;
  else
    return 0;
}

void order_by_sequence_num(single_data *ordered_data, size_t *total_size,
                           struct checkpoint_log *c_log) {
  for (int i = 0; i < c_log->variable_count; i++) {
    int data_index = c_log->c_data[i].version;
    for (int j = 0; j <= data_index; j++) {
      ordered_data[*total_size].address = c_log->c_data[i].address;
      ordered_data[*total_size].offset = c_log->c_data[i].offset;
      ordered_data[*total_size].data = malloc(c_log->c_data[i].size[j]);
      memcpy(ordered_data[*total_size].data, c_log->c_data[i].data[j],
             c_log->c_data[i].size[j]);
      ordered_data[*total_size].size = c_log->c_data[i].size[j];
      ordered_data[*total_size].version = j;
     // ordered_data[*total_size].sequence_number =
     //     c_log->c_data[i].sequence_number[j] ;
      ordered_data[*total_size].sequence_number =
          c_log->c_data[i].sequence_number[j] - c_log->c_data[i].sequence_number[0] ;
      printf("sequence number is %d address is %p\n", 
              ordered_data[*total_size].sequence_number,
              ordered_data[*total_size].address);
      // Adding in old versions to each single data structure to make reversion
      // simpler
      for (int k = 0; k < j; k++) {
        ordered_data[*total_size].old_data[k] =
            malloc(c_log->c_data[i].size[k]);
        memcpy(ordered_data[*total_size].old_data[k], c_log->c_data[i].data[k],
               c_log->c_data[i].size[k]);
        ordered_data[*total_size].old_size[k] = c_log->c_data[i].size[k];
      }
      *total_size = *total_size + 1;
    }
  }
  qsort(ordered_data, *total_size, sizeof(single_data), sequence_comparator);
  // printf("seq num total size is %ld\n", *total_size);
}
