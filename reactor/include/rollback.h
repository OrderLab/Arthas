// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _REACTOR_ROLLBACK_H_
#define _REACTOR_ROLLBACK_H_

#include <unistd.h>
#include "checkpoint.h"
#include <sys/mman.h>
#include <sys/fcntl.h>
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
                                 int num_data, seq_log *s_log);

int re_execute(const char *rexecution_cmd, int version_num,
               struct checkpoint_log *c_log, int num_data,
               const char *path, const char *layout,
               int reversion_type, int seq_num,
               void *old_pop, seq_log *s_log,
               const char *pmem_library);

void revert_by_address(const void *search_address, const void *address,
                       int variable_index, int version, int type, size_t size,
                       struct checkpoint_log *c_log);

int search_for_address(const void *address, size_t size,
                       struct checkpoint_log *c_log);

void revert_by_sequence_number(single_data search_data, int seq_num,
                               int rollback_version, seq_log *s_log);

void revert_by_offset(uint64_t search_offset, const void *address,
                      int variable_index, int version, int type, size_t size,
                      struct checkpoint_log *c_log);

struct node *search_for_offset(uint64_t old_off, checkpoint_log *c_log);

int checkpoint_hashcode(checkpoint_log *c_log, uint64_t offset);

void revert_by_sequence_number_checkpoint(checkpoint_data old_check_data,
                                          int rollback_version,
                                          single_data search_data);

void sort_by_sequence_number(void **addresses, size_t total_size, int num_data,
                             void **pmem_addresses,
                             void **sorted_pmem_addresses, uint64_t *offsets,
                             seq_log *s_log);

void seq_coarse_grain_reversion(int seq_num, void *pop, void *old_pop,
                                struct checkpoint_log *c_log, seq_log *s_log);

void revert_by_sequence_number_array(seq_log *s_log, int *seq_numbers,
                                     int total_seq_num,
                                     struct checkpoint_log *c_log);

int reverse_cmpfunc(const void *a, const void *b);

void decision_func_sequence_array(int *old_seq_numbers, int old_total,
                                  int *new_seq_numbers, int *new_total);

void revert_by_sequence_number_nonslice(void *old_pop, single_data ordered_data,
                                        int seq_num, int rollback_version,
                                        void *pop);

void undo_by_sequence_number(single_data search_data, int seq_num);
void revert_by_transaction(void **sorted_pmem_addresses, struct tx_log *t_log,
                           int *seq_numbers, int total_seq_num, seq_log *s_log);
#ifdef __cplusplus
}
#endif

#endif /* _REACTOR_ROLLBACK_H_ */
