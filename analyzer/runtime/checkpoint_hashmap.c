/*
 * The Arthas Project
 *
 * Copyright (c) 2019, Johns Hopkins University - Order Lab.
 *
 *    All rights reserved.
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *
 */

#include "checkpoint_hashmap.h"

struct checkpoint_log *c_log;
int variable_count = 0;
void *pmem_file_ptr;
void *checkpoint_file_curr;
void *checkpoint_file_address;

pthread_mutex_t mutex;
struct pool_info settings;
int non_checkpoint_flag = 0;
int sequence_number = 0;
size_t mapped_len;
uint64_t total_alloc = 0;
void *mmap_address = NULL;

void
init_checkpoint_log()
{
	// return;
	printf("init pmem checkpoint\n");
	if (c_log)
		return;
	non_checkpoint_flag = 1;
	c_log = malloc(sizeof(struct checkpoint_log));
	int is_pmem;
	if ((c_log = (struct checkpoint_log *)pmem_map_file(
		     "/mnt/pmem/pmem_checkpoint.pm", PMEM_LENGTH,
		     PMEM_FILE_CREATE, 0666, &mapped_len, &is_pmem)) == NULL) {
		perror("pmem_map_file");
		exit(1);
	}
	c_log->variable_count = 0;

	checkpoint_file_address = c_log;
	checkpoint_file_curr = (void *)((uint64_t)checkpoint_file_address +
					sizeof(struct checkpoint_log));
	void *old_pool_ptr = (void *)checkpoint_file_address;
	uint64_t old_pool = (uint64_t)old_pool_ptr;
	memcpy(checkpoint_file_curr, &old_pool, (sizeof(uint64_t)));
	checkpoint_file_curr =
		(void *)((uint64_t)checkpoint_file_curr + sizeof(uint64_t));
	c_log->list = checkpoint_file_curr;
	printf("c_log->list is %p %ld\n", c_log->list,
	       (uint64_t)c_log->list - (uint64_t)c_log);
	checkpoint_file_curr =
		(void *)((uint64_t)checkpoint_file_curr + sizeof(c_log->list));
	c_log->size = 5000010;
	for (int i = 0; i < (int)c_log->size; i++) {
		c_log->list[i] = NULL;
		checkpoint_file_curr = (void *)((uint64_t)checkpoint_file_curr +
						sizeof(struct node *));
	}
	if (is_pmem)
		pmem_persist(checkpoint_file_address, mapped_len);
	else
		pmem_msync(checkpoint_file_address, mapped_len);

	non_checkpoint_flag = 0;
}

void mmap_set(void *address){
  mmap_address = address;
}

uint64_t calculate_offset(void *address){
  uint64_t mmap_offset = (uint64_t)address - (uint64_t)mmap_address;
  return mmap_offset;
}

int
check_flag()
{
	return non_checkpoint_flag;
}

void
shift_to_left(struct node *found_node)
{
	non_checkpoint_flag = 1;
	if (c_log == NULL) {
		return;
	}
	for (int i = 0; i < MAX_VERSIONS - 1; i++) {
		found_node->c_data.data[i] = checkpoint_file_curr;
		checkpoint_file_curr = (void *)((uint64_t)checkpoint_file_curr +
						found_node->c_data.size[i + 1]);
		memcpy(found_node->c_data.data[i],
		       found_node->c_data.data[i + 1],
		       found_node->c_data.size[i + 1]);
		found_node->c_data.size[i] = found_node->c_data.size[i + 1];
		found_node->c_data.sequence_number[i] =
			found_node->c_data.sequence_number[i + 1];
	}
	non_checkpoint_flag = 0;
}

void
insert_value(const void *address, size_t size, const void *data_address,
	     uint64_t offset, int tx_id)
{
	non_checkpoint_flag = 1;
	 printf("INSERT VALUE value of size %ld offset is %ld seq num is %d addr is %p\n", 
         size, offset, sequence_number, address);
	if (c_log == NULL || address == NULL) {
		return;
	}

	// Look for address in hashmap
	struct node *found_node = lookup(offset);
	struct checkpoint_data insert_data;
	if (found_node == NULL) {
		// We need to insert node for address
		c_log->variable_count = c_log->variable_count + 1;
		variable_count = variable_count + 1;
		insert_data.address = address;
		insert_data.offset = offset;
		insert_data.size[0] = size;
		insert_data.version = 0;
		insert_data.tx_id[0] = tx_id;
		insert_data.sequence_number[0] = sequence_number;
		__atomic_fetch_add(&sequence_number, 1, __ATOMIC_SEQ_CST);
		insert_data.data[0] = checkpoint_file_curr;
		// pthread_mutex_lock(&mutex);
		checkpoint_file_curr =
			(void *)((uint64_t)checkpoint_file_curr + size);
		memcpy(insert_data.data[0], data_address, size);
		// pthread_mutex_unlock(&mutex);
		insert(offset, insert_data);
	} else if (found_node->c_data.data_type == -1) {
		c_log->variable_count = c_log->variable_count + 1;
		variable_count = variable_count + 1;
		insert_data.address = address;
		insert_data.offset = offset;
		insert_data.size[0] = size;
		insert_data.version = 0;
		insert_data.tx_id[0] = tx_id;
		insert_data.sequence_number[0] = sequence_number;
		__atomic_fetch_add(&sequence_number, 1, __ATOMIC_SEQ_CST);
		insert_data.data[0] = checkpoint_file_curr;
		// pthread_mutex_lock(&mutex);
		checkpoint_file_curr =
			(void *)((uint64_t)checkpoint_file_curr + size);
		memcpy(insert_data.data[0], data_address, size);
		// pthread_mutex_unlock(&mutex);
		insert(offset, insert_data);
	} else {
		if (found_node->c_data.version + 1 == MAX_VERSIONS) {
			// shift_to_left(found_node);
		} else {
			found_node->c_data.version += 1;
		}
		int data_index = found_node->c_data.version;
		found_node->c_data.address = address;
		found_node->c_data.size[data_index] = size;
		found_node->c_data.data[data_index] = checkpoint_file_curr;
		found_node->c_data.tx_id[data_index] = tx_id;
		// pthread_mutex_lock(&mutex);
		checkpoint_file_curr =
			(void *)((uint64_t)checkpoint_file_curr + size);
		memcpy(found_node->c_data.data[data_index], data_address, size);
		// pthread_mutex_unlock(&mutex);
		found_node->c_data.sequence_number[data_index] =
			sequence_number;
		__atomic_fetch_add(&sequence_number, 1, __ATOMIC_SEQ_CST);
	}
	non_checkpoint_flag = 0;
	// print_checkpoint_log();
}

void
checkpoint_free(uint64_t off)
{
	// printf("off %ld\n", off);
	struct node *temp = lookup(off);
	if (!temp)
		return;
	temp->c_data.free_flag = 1;
}

int
hashCode(uint64_t offset)
{
	int ret_val;
	if (offset < 0) {
		ret_val = (int)(offset % c_log->size);
		ret_val = -ret_val;
	}
	ret_val = (int)(offset % c_log->size);
	return ret_val;
}

void
insert(uint64_t offset, struct checkpoint_data c_data)
{
	int pos = hashCode(offset);
	struct node *list = c_log->list[pos];
	struct node *temp = list;
	uint64_t old_offset = 0;
	while (temp) {
		if (temp->offset == offset) {
			if (temp->c_data.old_checkpoint_entry != 0) {
				old_offset = temp->c_data.old_checkpoint_entry;
			}
			temp->c_data = c_data;
			temp->c_data.old_checkpoint_entry = old_offset;
			return;
		}
		temp = temp->next;
	}
	// Need to create a new insertion
	//   pthread_mutex_lock(&mutex);
	struct node *newNode = (struct node *)checkpoint_file_curr;
	checkpoint_file_curr =
		(void *)((uint64_t)checkpoint_file_curr + sizeof(struct node));
	//   pthread_mutex_unlock(&mutex);
	newNode->c_data = c_data;
	newNode->next = list;
	newNode->offset = offset;
	c_log->list[pos] = newNode;
}

struct node *
lookup(uint64_t offset)
{
	int pos = hashCode(offset);
	struct node *list = c_log->list[pos];
	struct node *temp = list;
	while (temp) {
		if (temp->offset == offset) {
			return temp;
		}
		temp = temp->next;
	}
	return NULL;
}

bool
check_pmem(void *addr, size_t size)
{
	uint64_t address = (uint64_t)addr;
	uint64_t pmem_base = (uint64_t)checkpoint_file_address;
	printf("address is %ld, pmem_base is %ld, mapped is %ld\n", address,
	       pmem_base, mapped_len);
	if (address >= pmem_base && address <= (pmem_base + mapped_len)) {
		return true;
	}
	return false;
}

bool
check_offset(uint64_t offset)
{
	if (offset < mapped_len)
		return true;
	return false;
}

void
print_checkpoint_log()
{
	printf("**************\n\n");
	struct node *list;
	struct node *temp;
	printf("c_log is %p\n", c_log);
	printf("c_log->list is %p\n", c_log->list);
	for (int i = 0; i < (int)c_log->size; i++) {
		list = c_log->list[i];
		temp = list;
		while (temp) {
			printf("position is %d\n", i);
			printf("address is %p offset is %ld\n",
			       temp->c_data.address, temp->offset);
			int data_index = temp->c_data.version;
			printf("number of versions is %d\n",
			       temp->c_data.version);
			printf("old checkpoint %ld new cp %ld\n",
			       temp->c_data.old_checkpoint_entry,
			       temp->c_data.new_checkpoint_entry);
			for (int j = 0; j <= data_index; j++) {
				printf("version is %d size is %ld seq num is %d value is %f or %d or %s\n",
				       j, temp->c_data.size[j],
				       temp->c_data.sequence_number[j],
				       *((double *)temp->c_data.data[j]),
				       *((int *)temp->c_data.data[j]),
				       (char *)temp->c_data.data[j]);
				printf("tx pointer is %d\n",
				       temp->c_data.tx_id[j]);
			}
			temp = temp->next;
		}
	}
	printf("the variable count is %d %d\n", variable_count,
	       c_log->variable_count);
}
