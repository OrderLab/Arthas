#ifndef CHECKPOINT_GENERIC_H
#define CHECKPOINT_GENERIC_H 1

#include <stdio.h>
#include <stdlib.h>
#include "libpmemobj.h"

#define MAX_VARIABLES 1000
#define MAX_VERSIONS 3

#define INT_CHECKPOINT 0
#define DOUBLE_CHECKPOINT 1
#define STRING_CHECKPOINT 2
#define BOOL_CHECKPOINT 3

struct checkpoint_data {
  const void *address;
  uint64_t offset;
  void *data[MAX_VERSIONS];
  size_t size[MAX_VERSIONS];
  int version;
  int data_type;
};

struct checkpoint_log{
  struct checkpoint_data c_data[MAX_VARIABLES];
  int variable_count;
};

int search_for_address(const void * address, size_t size, struct checkpoint_log * c_log){
  uint64_t uint_address = (uint64_t)address;
  uint64_t search_upper_bound  = uint_address + size;
  uint64_t clog_upper_bound;
  for(int i = 0; i < c_log->variable_count; i++){
    uint64_t clog_address = (uint64_t)c_log->c_data[i].address;
    clog_upper_bound = clog_address + c_log->c_data[i].size;
    if(uint_address >= clog_address && search_upper_bound <= clog_upper_bound){
      return i;
    }
    /*if(c_log->c_data[i].address == address){
      return i;
    }*/
  }
  return -1;
}

void revert_by_address(const void *address, int variable_index, int version, int type, size_t size){
  void *dest = (void *)address;

}

#endif
