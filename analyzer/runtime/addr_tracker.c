// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "addr_tracker.h"

FILE *__arthas_tracker_file;
#define MAX_ADDRESSES 2000000001
#define FLUSH_LIMIT 100000
#define MAX_FILE_NAME_SIZE 128

// void *addresses[MAX_ADDRESSES];
// unsigned guids[MAX_ADDRESSES];
void **addresses;
unsigned *guids;
int address_count = 0;
// int guid_count = 0;
pthread_mutex_t lock;

void *myThreadFun(void *vargp) {
  printf("start background thread\n");
  while (1) {
    while (address_count < FLUSH_LIMIT) {
      // printf("stuck in here\n");
      // spinlock here
    }
    // printf("outside\n");
    // pthread_mutex_lock(&lock);
    for (int i = 0; i < address_count; i++)
      fprintf(__arthas_tracker_file, "%p,%u\n", addresses[i], guids[i]);
    address_count = 0;
    // pthread_mutex_unlock(&lock);
    // printf("address count is %d\n", address_count);
  }
}

char *__arthas_tracker_file_name(char *buf) {
  snprintf(buf, 128, "pmem_addr_pid_%d.dat", getpid());
  return buf;
}

void termination_handler(int signum) {
  /*for(int i = 0; i < address_count; i++){
    fprintf(__arthas_tracker_file, "%p,%u\n", addresses[i], guids[i]);
  }*/
  fclose(__arthas_tracker_file);
  exit(-1);
}

void __arthas_low_level_init() {
  // init_checkpoint_log();
  // init_log();
  // printf("yes\n");
}

void __arthas_low_level_fence() {
  // increment_tx_id();
  // print_checkpoint_log();
}

void __arthas_low_level_flush(void *address) {
  printf("clflush address is %p\n", address);
  // insert_value(address, 64, address);
  printf("low level flush\n");
}

void __arthas_save_file(void *address) {
  printf("save file is %p\n", address);
  // save_pmem_file(address);
}

void __arthas_addr_tracker_init() {
  if (pthread_mutex_init(&lock, NULL) != 0) {
    printf("\n mutex init has failed\n");
    return 1;
  }
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, myThreadFun, NULL);
  char filename_buf[MAX_FILE_NAME_SIZE];
  char *filename = __arthas_tracker_file_name(filename_buf);
  fprintf(stderr, "openning address tracker output file %s\n", filename);
  __arthas_tracker_file = fopen(filename, "w");
  addresses = malloc(sizeof(void *) * MAX_ADDRESSES);
  guids = malloc(sizeof(unsigned int) * MAX_ADDRESSES);

  struct sigaction new_action, old_action;
  new_action.sa_handler = termination_handler;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = 0;
  new_action.sa_sigaction = &termination_handler;

  sigaction(SIGINT, &new_action, NULL);
  sigaction(SIGFPE, &new_action, NULL);
  sigaction(SIGTERM, &new_action, NULL);
  sigaction(SIGSEGV, &new_action, NULL);
  sigaction(SIGILL, &new_action, NULL);
  sigaction(SIGABRT, &new_action, NULL);
  sigaction(SIGBUS, &new_action, NULL);
  sigaction(SIGSTKFLT, &new_action, NULL);
}

inline void __arthas_track_addr(char *addr, unsigned int guid) {
  // TODO: replace the fprintf with buffering and async write
  // fprintf(__arthas_tracker_file, "%p,%u\n", addr, guid);
  // pthread_mutex_lock(&lock);
  if (address_count > MAX_ADDRESSES) {
    printf("too large %d\n", address_count);
    pthread_mutex_lock(&lock);
    pthread_mutex_unlock(&lock);
  }
  addresses[address_count] = addr;
  guids[address_count] = guid;
  address_count++;
  /*if(address_count >= 10){
    for(int i = 0; i < 10; i++)
      fprintf(__arthas_tracker_file, "%p,%u\n", addresses[i], guids[i]);
    address_count = 0;
  }*/
}

/*inline void __arthas_track_addr(char **addresses, unsigned int *guids,
                                int address_count) {
  // TODO: replace the fprintf with buffering and async write
  for(int i = 0; i < address_count; i++){
    fprintf(__arthas_tracker_file, "%p,%u\n", addresses[i], guids[i]);
  }
  //fprintf(__arthas_tracker_file, "%p,%u\n", addr, guid);
  //guids[guid_count] = guid;
  //guid_count;
}*/

bool __arthas_addr_tracker_dump() {
  fflush(__arthas_tracker_file);
  return true;
}

void __arthas_addr_tracker_finish() {
  // close the tracker file
  for (int i = 0; i < address_count; i++)
    fprintf(__arthas_tracker_file, "%p,%u\n", addresses[i], guids[i]);
  address_count = 0;
  fflush(__arthas_tracker_file);
  // fclose(__arthas_tracker_file);
}
