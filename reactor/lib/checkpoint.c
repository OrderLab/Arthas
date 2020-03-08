// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "checkpoint.h"

struct checkpoint_log *reconstruct_checkpoint(const char *file_path) {
  PMEMobjpool *pop = pmemobj_open(file_path, "checkpoint");
  if (!pop) {
    fprintf(stderr, "pool not found\n");
    pmemobj_errormsg();
    return NULL;
  }
  PMEMoid oid = pmemobj_root(pop, sizeof(uint64_t));
  uint64_t *old_pool = (uint64_t *)pmemobj_direct(oid);
  // printf("old pool is %p\n", (void *)*old_pool);
  // cout << "old pool " << *old_pool << "\n";
  struct checkpoint_log *c_log;
  PMEMoid clog_oid = POBJ_FIRST_TYPE_NUM(pop, 0);
  c_log = (struct checkpoint_log *)pmemobj_direct(clog_oid);
  // printf("c log variables is %d\n", c_log->variable_count);
  // cout << "c log c data " << c_log->c_data[0].version << "\n";

  uint64_t offset;
  offset = (uint64_t)c_log->c_data - *old_pool;
  int variable_count = c_log->variable_count;
  for (int i = 0; i < variable_count; i++) {
    for (int j = 0; j <= c_log->c_data[i].version; j++) {
      offset = (uint64_t)c_log->c_data[i].data[j] - *old_pool;
      // printf("offset is %ld\n", offset);
      c_log->c_data[i].data[j] = (void *)((uint64_t)pop + offset);
    }
  }

  printf("RECONSTRUCTED CHECKPOINT COMPONENT:\n");
  for (int i = 0; i < variable_count; i++) {
    printf("address is %p\n", c_log->c_data[i].address);
    // printf("version is %d\n", c_log->c_data[i].version);
    int data_index = c_log->c_data[i].version;
    for (int j = 0; j <= data_index; j++) {
      printf("version is %d ", j);
      if (c_log->c_data[i].size[0] == 4)
        printf("value is %d\n", *((int *)c_log->c_data[i].data[j]));
      else
        printf("value is %f\n", *((double *)c_log->c_data[i].data[j]));
      // printf("version is %d, value is %f or %d\n", j, *((double
      // *)c_log->c_data[i].data[j]),*((int *)c_log->c_data[i].data[j]));
    }
  }
  *old_pool = (uint64_t)pop;
  return c_log;
}

