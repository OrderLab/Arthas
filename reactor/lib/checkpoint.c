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
  if (strcmp(pmem_library, "libpmemobj2") == 0) {
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
    offset = (uint64_t)c_log->list - (uint64_t)pop;

    offset = (uint64_t)c_log->list - *old_pool;
    variable_count = c_log->variable_count;
    printf("c log list offset is %ld\n", offset);
    c_log->list = (struct node **)((uint64_t)pop + offset);
    printf("variable count is %d\n", variable_count);

    struct node *list;
    struct node *temp;
    struct node *next_node;
    for (int i = 0; i < (int)c_log->size; i++) {
      list = c_log->list[i];
      temp = list;
      if (temp) {
        offset = (uint64_t)temp - *old_pool;
        temp = (struct node *)((uint64_t)pop + offset);
        c_log->list[i] = (struct node *)((uint64_t)pop + offset);
      }
      while (temp) {
        int data_index = temp->c_data.version;
        for (int j = 0; j <= data_index; j++) {
          offset = ((uint64_t)temp->c_data.data[j] - *old_pool);
          temp->c_data.data[j] = (void *)((uint64_t)pop + offset);
        }
        if (temp->next) {
          offset = (uint64_t)temp->next - *old_pool;
          temp->next = (struct node *)((uint64_t)pop + offset);
        }
        temp = temp->next;
      }
    }
    *old_pool = (uint64_t)pop;
  } else if (strcmp(pmem_library, "libpmem") == 0 ||
             strcmp(pmem_library, "libpmemobj") == 0 || 
             strcmp(pmem_library, "mmap") == 0) {
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
    offset = (uint64_t)c_log->list - old_pool;
    c_log->list = (struct node **)((uint64_t)pmemaddr + offset);
    struct node *list;
    struct node *temp;
    struct node *next_node;
    for (int i = 0; i < (int)c_log->size; i++) {
      list = c_log->list[i];
      temp = list;
      if (temp) {
        offset = (uint64_t)temp - old_pool;
        temp = (struct node *)((uint64_t)pmemaddr + offset);
        c_log->list[i] = (struct node *)((uint64_t)pmemaddr + offset);
      }
      while (temp) {
        int data_index = temp->c_data.version;
        for (int j = 0; j <= data_index; j++) {
          offset = ((uint64_t)temp->c_data.data[j] - old_pool);
          temp->c_data.data[j] = (void *)((uint64_t)pmemaddr + offset);
        }
        if (temp->next) {
          offset = (uint64_t)temp->next - old_pool;
          temp->next = (struct node *)((uint64_t)pmemaddr + offset);
        }
        temp = temp->next;
      }
    }
    *old_pool_ptr = (uint64_t)pmemaddr;
  }
  if (c_log == NULL) {
    return NULL;
  }
  printf("RECONSTRUCTED CHECKPOINT COMPONENT:\n");
  printf("variable count is %d\n", variable_count);
   //print_checkpoint_log(c_log);
  return c_log;
}

void print_checkpoint_log(checkpoint_log *c_log) {
  printf("**************\n\n");
  struct node *list;
  struct node *temp;
  for (int i = 0; i < (int)c_log->size; i++) {
    list = c_log->list[i];
    temp = list;
    while (temp) {
      printf("inside %d\n", i);
      printf("temp is %p\n", temp);
      printf("address is %p offset is %ld\n", temp->c_data.address,
             temp->offset);
      int data_index = temp->c_data.version;
      printf("number of versions is %d\n", temp->c_data.version);
      for (int j = 0; j <= data_index; j++) {
        printf("version is %d size is %ld value is %f or %d %s\n", j,
               temp->c_data.size[j], *((double *)temp->c_data.data[j]),
               *((int *)temp->c_data.data[j]), (char *)temp->c_data.data[j]);
        printf("seq num is %d\n", temp->c_data.sequence_number[j]);
      }
      temp = temp->next;
    }
  }
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

void order_by_tx_id(tx_log *t_log, struct checkpoint_log *c_log) {
  struct node *list;
  struct node *temp;
  single_data ordered_data;
  for (int i = 0; i < (int)c_log->size; i++) {
    list = c_log->list[i];
    temp = list;
    while (temp) {
      int data_index = temp->c_data.version;
      for (int j = 0; j <= data_index; j++) {
        int seq_num = temp->c_data.sequence_number[j];
        ordered_data.address = temp->c_data.address;
        ordered_data.offset = temp->offset;
        ordered_data.data = malloc(temp->c_data.size[j]);
        memcpy(ordered_data.data, temp->c_data.data[j], temp->c_data.size[j]);
        ordered_data.size = temp->c_data.size[j];
        ordered_data.version = j;
        ordered_data.sequence_number = temp->c_data.sequence_number[j];
        ordered_data.old_checkpoint_entry = temp->c_data.old_checkpoint_entry;
        ordered_data.tx_id = temp->c_data.tx_id[j];
        for (int k = 0; k < j; k++) {
          ordered_data.old_data[k] = malloc(temp->c_data.size[k]);
          memcpy(ordered_data.old_data[k], temp->c_data.data[k],
                 temp->c_data.size[k]);
          ordered_data.old_size[k] = temp->c_data.size[k];
        }
        tx_insert(t_log, ordered_data.tx_id, ordered_data);
      }
      temp = temp->next;
    }
  }
}

void order_by_sequence_num(seq_log *s_log, size_t *total_size,
                           struct checkpoint_log *c_log) {
  struct node *list;
  struct node *temp;
  item *it;
  single_data ordered_data;
  for (int i = 0; i < (int)c_log->size; i++) {
    list = c_log->list[i];
    temp = list;
    while (temp) {
      int data_index = temp->c_data.version;
      for (int j = 0; j <= data_index; j++) {
        int seq_num = temp->c_data.sequence_number[j];
        ordered_data.address = temp->c_data.address;
        ordered_data.offset = temp->offset;
        ordered_data.data = malloc(temp->c_data.size[j]);
        printf("data is %p size is %ld\n", ordered_data.data, temp->c_data.size[j]);
        memcpy(ordered_data.data, temp->c_data.data[j], temp->c_data.size[j]);
        ordered_data.size = temp->c_data.size[j];
        ordered_data.version = j;
        ordered_data.sequence_number = temp->c_data.sequence_number[j];
        ordered_data.old_checkpoint_entry = temp->c_data.old_checkpoint_entry;
        ordered_data.tx_id = temp->c_data.tx_id[j];
        for (int k = 0; k < j; k++) {
          ordered_data.old_data[k] = malloc(temp->c_data.size[k]);
          memcpy(ordered_data.old_data[k], temp->c_data.data[k],
                 temp->c_data.size[k]);
          ordered_data.old_size[k] = temp->c_data.size[k];
        }
        *total_size = *total_size + 1;
        insert(s_log, seq_num, ordered_data);
      }
      temp = temp->next;
    }
  }
}

int txhashCode(tx_log *t_log, int key) {
  if (key < 0) return -(key % t_log->size);
  return key % t_log->size;
}

void tx_insert(tx_log *t_log, int key, single_data ordered_data) {
  int pos = txhashCode(t_log, key);
  struct tx_node *list = t_log->list[pos];
  struct tx_node *newNode = (struct tx_node *)malloc(sizeof(struct tx_node));
  struct tx_node *temp = list;
  while (temp) {
    /*if(temp->tx_id==key){
        temp->ordered_data = ordered_data;
        return;
    }*/
    temp = temp->next;
  }
  newNode->tx_id = key;
  newNode->ordered_data = ordered_data;
  newNode->next = list;
  // printf("insertion to position %d\n", pos);
  t_log->list[pos] = newNode;
}

single_data tx_lookup(tx_log *t_log, int key) {
  int pos = txhashCode(t_log, key);
  struct tx_node *list = t_log->list[pos];
  struct tx_node *temp = list;
  while (temp) {
    if (temp->tx_id == key) {
      return temp->ordered_data;
    }
    temp = temp->next;
  }
  single_data error_data;
  error_data.tx_id = -1;
  return error_data;
}

int hashCode(seq_log *s_log, int key) {
  if (key < 0) return -(key % s_log->size);
  return key % s_log->size;
}

void insert(seq_log *s_log, int key, single_data ordered_data) {
  struct seq_node *t = s_log->list[92];
  int pos = hashCode(s_log, key);
  struct seq_node *list = s_log->list[pos];
  struct seq_node *newNode = (struct seq_node *)malloc(sizeof(struct seq_node));
  struct seq_node *temp = list;
  while (temp) {
    if (temp->sequence_number == key) {
      temp->ordered_data = ordered_data;
      return;
    }
    temp = temp->next;
  }
  newNode->sequence_number = key;
  newNode->ordered_data = ordered_data;
  newNode->next = list;

  s_log->list[pos] = newNode;
}

int rev_lookup(seq_log *s_log, int key) {
  int pos = hashCode(s_log, key);
  struct seq_node *list = s_log->list[pos];
  struct seq_node *temp = list;
  while (temp) {
    if (temp->sequence_number == key) return 1;
    temp = temp->next;
  }
  return -1;
}

void lookup_modify(seq_log *s_log, int key, void *addr) {
  int pos = hashCode(s_log, key);
  struct seq_node *list = s_log->list[pos];
  struct seq_node *temp = list;
  while (temp) {
    if (temp->sequence_number == key) {
      temp->ordered_data.sorted_pmem_address = addr;
      return;
    }
    temp = temp->next;
  }
}

void lookup_undo_save(seq_log *s_log, int key, void *addr, size_t size) {
  int pos = hashCode(s_log, key);
  struct seq_node *list = s_log->list[pos];
  struct seq_node *temp = list;
  while (temp) {
    if (temp->sequence_number == key) {
      int atemp = 1;
      memcpy(temp->ordered_data.data, &atemp, sizeof(int));
      printf("after first memcpy %p\n", temp->ordered_data.sorted_pmem_address);
      memcpy(temp->ordered_data.sorted_pmem_address, &atemp, sizeof(int));
      printf("temp->ordered_data data %p %ld\n", temp->ordered_data.data, size);
      memcpy(temp->ordered_data.data, temp->ordered_data.sorted_pmem_address,
             size);
      printf("after memcpy\n");
      return;
    }
    temp = temp->next;
  }
}

single_data lookup(seq_log *s_log, int key) {
  int pos = hashCode(s_log, key);
  struct seq_node *list = s_log->list[pos];
  struct seq_node *temp = list;
  while (temp) {
    if (temp->sequence_number == key) {
      return temp->ordered_data;
    }
    temp = temp->next;
  }
  single_data error_data;
  error_data.sequence_number = -1;
  return error_data;
}

int *address_lookup(seq_log *s_log, uint64_t address, int *seq_count,
                    int *sequences) {
  struct seq_node *list;
  struct seq_node *temp;
  *seq_count = 0;
  for (int i = 0; i < (int)s_log->size; i++) {
    list = s_log->list[i];
    temp = list;
    while (temp) {
      if (address == (uint64_t)temp->ordered_data.address) {
        sequences[*seq_count] = temp->sequence_number;
        *seq_count = *seq_count + 1;
      }
      temp = temp->next;
    }
  }
  return sequences;
}

int count_higher(seq_log *s_log, int seq_num) {
  struct seq_node *list;
  struct seq_node *temp;
  int return_number = 0;
  for (int i = 0; i < (int)s_log->size; i++) {
    list = s_log->list[i];
    temp = list;
    while (temp) {
      if (temp->sequence_number > seq_num) {
        return_number++;
      }
      temp = temp->next;
    }
  }
  return return_number;
}

int find_highest_seq_num(seq_log *s_log) {
  struct seq_node *list;
  struct seq_node *temp;
  int highest_seq_num = -1;
  for (int i = 0; i < (int)s_log->size; i++) {
    list = s_log->list[i];
    temp = list;
    while (temp) {
      if (temp->sequence_number > highest_seq_num) {
        highest_seq_num = temp->sequence_number;
      }
      temp = temp->next;
    }
  }
  return highest_seq_num;
}

int offsethashCode(uint64_t offset, checkpoint_log *c_log) {
  int ret_val;
  if (offset < 0) {
    ret_val = (int)(offset % c_log->size);
    ret_val = -ret_val;
  }
  ret_val = (int)(offset % c_log->size);
  return ret_val;
}

struct node *
lookup_clog(uint64_t offset, checkpoint_log *c_log)
{
        int pos = offsethashCode(offset, c_log);
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

