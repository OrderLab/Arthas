// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _REACTOR_ROLLBACK_H_
#define _REACTOR_ROLLBACK_H_

#include "checkpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_COARSE_ATTEMPTS 5
#define MAX_DATA 1000

#define COARSE_GRAIN_NAIVE 1
#define COARSE_GRAIN_SEQUENCE 2
#define FINE_GRAIN 3

// definition is in rollback.c
extern int coarse_grained_tries;
extern int fine_grained_tries;

void coarse_grain_reversion(void **addresses, struct checkpoint_log *c_log,
                            void **pmem_addresses, int version_num,
                            int num_data, uint64_t *offsets);

PMEMobjpool *redo_pmem_addresses(const char *path, const char *layout,
                                 int num_data, void **pmem_addresses,
                                 uint64_t *offsets);

int re_execute(const char *rexecution_cmd, int version_num, void **addresses,
               struct checkpoint_log *c_log, void **pmem_addresses,
               int num_data, const char *path, const char *layout,
               uint64_t *offsets, int reversion_type, int seq_num,
               void **sorted_pmem_addresses, single_data *ordered_data);

void revert_by_address(const void *search_address, const void *address,
                       int variable_index, int version, int type, size_t size,
                       struct checkpoint_log *c_log);

int search_for_address(const void *address, size_t size,
                       struct checkpoint_log *c_log);

void revert_by_sequence_number(void **sorted_pmem_addresses,
                               single_data *ordered_data, int seq_num,
                               int rollback_version);

void revert_by_offset(uint64_t search_offset, const void *address,
                      int variable_index, int version, int type, size_t size,
                      struct checkpoint_log *c_log);

int search_for_offset(uint64_t offset, size_t size,
                      struct checkpoint_log *c_log);

void sort_by_sequence_number(void **addresses, single_data *ordered_data,
                             size_t total_size, int num_data,
                             void **sorted_addresses, void **pmem_addresses,
                             void **sorted_pmem_addresses, uint64_t *offsets);

void seq_coarse_grain_reversion(uint64_t *offsets, void **sorted_pmem_addresses,
                                int seq_num, single_data *ordered_data);

void revert_by_sequence_number(void **sorted_pmem_addresses,
                               single_data *ordered_data, int seq_num,
                               int rollback_version);

#ifdef __cplusplus
}
#endif

#endif /* _REACTOR_ROLLBACK_H_ */
