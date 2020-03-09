// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _REACTOR_CHECKPOINT_H_
#define _REACTOR_CHECKPOINT_H_

#include <libpmemobj.h>
#include <libpmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_VARIABLES 1000
#define MAX_VERSIONS 3
#define PMEM_LEN 200000

#define INT_CHECKPOINT 0
#define DOUBLE_CHECKPOINT 1
#define STRING_CHECKPOINT 2
#define BOOL_CHECKPOINT 3

//checkpoint log entry by logical sequence number
typedef struct single_data {
  const void *address;
  uint64_t offset;
  void *data;
  size_t size;
  int sequence_number;
  int version;
  int data_type;
  void *old_data[MAX_VERSIONS];
  size_t old_size[MAX_VERSIONS];
} single_data;

// checkpoint log entry by address/offset
typedef struct checkpoint_data {
  const void *address;
  uint64_t offset;
  void *data[MAX_VERSIONS];
  size_t size[MAX_VERSIONS];
  int version;
  int data_type;
  int sequence_number[MAX_VERSIONS];
} checkpoint_data;

typedef struct checkpoint_log {
  struct checkpoint_data c_data[MAX_VARIABLES];
  int variable_count;
} checkpoint_log;

struct checkpoint_log *reconstruct_checkpoint(const char *file_path, const char *pmem_library);
void order_by_sequence_num(struct single_data * ordered_data, size_t *total_size, struct checkpoint_log *c_log);
int sequence_comparator(const void *v1, const void * v2);

#ifdef __cplusplus
}
#endif

#endif /* _REACTOR_CHECKPOINT_H_ */
