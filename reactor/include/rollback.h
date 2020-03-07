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

// definition is in rollback.c
extern int coarse_grained_tries;
extern int fine_grained_tries;

void coarse_grain_reversion(void **addresses, struct checkpoint_log *c_log,
                            void **pmem_addresses, int version_num,
                            int num_data);

PMEMobjpool *redo_pmem_addresses(const char *path, const char *layout,
                                 int num_data, void **pmem_addresses,
                                 uint64_t *offsets);

void re_execute(char **reexecution_lines, int version_num, int line_counter,
                void **addresses, struct checkpoint_log *c_log,
                void **pmem_addresses, int num_data, const char *path,
                const char *layout, uint64_t *offsets);

void revert_by_address(const void *search_address, const void *address,
                       int variable_index, int version, int type, size_t size,
                       struct checkpoint_log *c_log);

int search_for_address(const void *address, size_t size,
                       struct checkpoint_log *c_log);

#ifdef __cplusplus
}
#endif

#endif /* _REACTOR_ROLLBACK_H_ */
