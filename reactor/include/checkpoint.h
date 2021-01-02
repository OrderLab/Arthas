// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _REACTOR_CHECKPOINT_H_
#define _REACTOR_CHECKPOINT_H_

#include <libpmem.h>
#include <libpmemobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_VARIABLES 5000010
#define MAX_VERSIONS 3
#define PMEM_LEN 8388608000

#define INT_CHECKPOINT 0
#define DOUBLE_CHECKPOINT 1
#define STRING_CHECKPOINT 2
#define BOOL_CHECKPOINT 3

#define ITEM_CAS 2
#define ITEM_key(item)         \
  (((char *)&((item)->data)) + \
   (((item)->it_flags & ITEM_CAS) ? sizeof(uint64_t) : 0))
typedef unsigned int rel_time_t;
typedef struct _stritem {
  /* Protected by LRU locks */
  struct _stritem *next;
  struct _stritem *prev;
  /* Rest are protected by an item lock */
  struct _stritem *h_next; /* hash chain next */
  rel_time_t time;         /* least recent access */
  rel_time_t exptime;      /* expire time */
  int nbytes;              /* size of data */
  unsigned short refcount;
  uint8_t nsuffix;     /* length of flags-and-length string */
  uint8_t it_flags;    /* ITEM_* above */
  uint8_t slabs_clsid; /* which slab class we're in */
  uint8_t nkey;        /* key length, w/terminating null and padding */
  /* this odd type prevents type-punning issues when we do
   * the little shuffle to save space when not using CAS. */
  union {
    uint64_t cas;
    char end;
  } data[];
  /* if it_flags & ITEM_CAS we have 8 bytes CAS */
  /* then null-terminated key */
  /* then " flags length\r\n" (no terminating null) */
  /* then data with terminating \r\n (no terminating null; it's binary!) */
} item;

// checkpoint log entry by logical sequence number
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
  uint64_t old_checkpoint_entry;
  uint64_t new_checkpoint_entry;
  void *sorted_pmem_address;
  int tx_id;
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
  uint64_t old_checkpoint_entry;
  uint64_t new_checkpoint_entry;
  int free_flag;
  int tx_id[MAX_VERSIONS];
} checkpoint_data;

struct node {
  uint64_t offset;
  struct checkpoint_data c_data;
  struct node *next;
};

typedef struct checkpoint_log {
  size_t size;
  int variable_count;
  struct node **list;
} checkpoint_log;

struct tx_node {
  int tx_id;
  struct single_data ordered_data;
  struct tx_node *next;
};

typedef struct tx_log {
  size_t size;
  struct tx_node **list;
} tx_log;

struct seq_node {
  int sequence_number;
  struct single_data ordered_data;
  struct seq_node *next;
};

typedef struct seq_log {
  size_t size;
  struct seq_node **list;
} seq_log;

typedef struct rev_log {
  size_t size;
  struct seq_node *list;
} rev_log;

struct checkpoint_log *reconstruct_checkpoint(const char *file_path,
                                              const char *pmem_library);
void order_by_sequence_num(seq_log *s_log, size_t *total_size,
                           struct checkpoint_log *c_log);
void order_by_tx_id(tx_log *t_log, struct checkpoint_log *c_log);
int sequence_comparator(const void *v1, const void *v2);
void print_checkpoint_log(checkpoint_log *c_log);
int hashCode(seq_log *s_log, int key);
int txhashCode(tx_log *t_log, int key);
void tx_insert(tx_log *t_log, int key, single_data ordered_data);
void insert(seq_log *s_log, int key, single_data ordered_data);
single_data tx_lookup(tx_log *t_log, int key);
single_data lookup(seq_log *s_log, int key);
int find_highest_seq_num(seq_log *s_log);
int *address_lookup(seq_log *s_log, uint64_t address, int *seq_count,
                    int *sequences);
int rev_lookup(seq_log *s_log, int key);
void lookup_modify(seq_log *s_log, int key, void *addr);
void lookup_undo_save(seq_log *s_log, int key, void *addr, size_t size);
int count_higher(seq_log *s_log, int seq_num);
#ifdef __cplusplus
}
#endif

#endif /* _REACTOR_CHECKPOINT_H_ */
