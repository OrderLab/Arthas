// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef __ADDR_TRACKER_H_
#define __ADDR_TRACKER_H_

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "checkpoint_hashmap.h"
#include "libpmem.h"

#ifdef __cplusplus
extern "C" {
#endif

extern inline char *__arthas_tracker_file_name(char *buf);
extern inline void __arthas_track_addr(char *addr, unsigned int guid);
// extern inline void __arthas_track_addr(char **addresses, unsigned int *guids,
//                                        int address_count);

void __arthas_addr_tracker_init();
bool __arthas_addr_tracker_dump();
void __arthas_addr_tracker_finish();
void __arthas_low_level_init();
#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* __ADDR_TRACKER_H_ */
