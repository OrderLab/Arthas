/*
 * The Arthas Project
 *
 * Copyright (c) 2019, Johns Hopkins University - Order Lab.
 *
 *    All rights reserved.
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *
 */

#ifndef CHECKPOINT_HASHMAP_H
#define CHECKPOINT_HASHMAP_H 1

#define MAX_VARIABLES 5000010
#define MAX_VERSIONS 3
#define PMEM_LENGTH 8388608000
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "libpmem.h"
#include "libpmemobj.h"
#include <limits.h>
//#include "pmem.h"

#define INT_CHECKPOINT 0
#define DOUBLE_CHECKPOINT 1
#define STRING_CHECKPOINT 2
#define BOOL_CHECKPOINT 3

struct pool_info {
	// PMEMobjpool *pm_pool;
};

struct single_data {
	const void *address;
	uint64_t offset;
	void *data;
	size_t size;
	int sequence_number;
	int version;
	int data_type;
};

struct checkpoint_data {
	const void *address;
	uint64_t offset;
	void *data[MAX_VERSIONS];
	size_t size[MAX_VERSIONS];
	int version;
	int data_type;
	int sequence_number[MAX_VERSIONS];
	uint64_t old_checkpoint_entry;
	uint64_t new_checkpoint_entry;
	int free_flag;
	int tx_id[MAX_VERSIONS];
	// uint64_t old_checkpoint_entries[MAX_VERSIONS];
	// int old_checkpoint_counter;
};

/*struct checkpoint_log{
  struct checkpoint_data c_data[MAX_VARIABLES];
  int variable_count;
};*/

struct node {
	uint64_t offset;
	struct checkpoint_data c_data;
	struct node *next;
};

struct checkpoint_log {
	size_t size;
	int variable_count;
	struct node **list;
};

int hashCode(uint64_t offset);
void insert(uint64_t offset, struct checkpoint_data c_data);
struct node *lookup(uint64_t offset);
bool check_pmem(void *addr, size_t size);
bool check_offset(uint64_t offset);

extern struct checkpoint_log *c_log;
extern int variable_count;
extern void *pmem_file_ptr;
extern struct pool_info settings;
extern int non_checkpoint_flag;

void init_checkpoint_log(void);
int check_flag(void);
// void write_flag(char c);
void shift_to_left(struct node *found_node);
// int check_address_length(const void *address, size_t size);
// int search_for_offset(uint64_t pool_base, uint64_t offset);
// int search_for_address(const void *address);
void insert_value(const void *address, size_t size, const void *data_address,
		  uint64_t offset, int tx_id);
void print_checkpoint_log(void);
// void revert_by_address(const void *address, int variable_index, int version,
// int type, size_t size);
// int check_offset(uint64_t offset, size_t size);
// void revert_by_offset(const void *address, uint64_t offset, int
// variable_index, int version, int type, size_t size);
// void order_by_sequence_num(struct single_data * ordered_data, size_t
// *total_size);
// int sequence_comparator(const void *v1, const void * v2);
// void print_sequence_array(struct single_data *ordered_data, size_t
// total_size);
void checkpoint_realloc(void *new_ptr, void *old_ptr, uint64_t new_offset,
			uint64_t old_offset);
void checkpoint_free(uint64_t off);
void mmap_set(void *address);
uint64_t calculate_offset(void *address);
#endif
