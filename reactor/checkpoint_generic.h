#ifndef CHECKPOINT_GENERIC_H
#define CHECKPOINT_GENERIC_H 1

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

#endif
