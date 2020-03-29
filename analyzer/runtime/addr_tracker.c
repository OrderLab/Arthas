// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "addr_tracker.h"

FILE *__arthas_tracker_file;

#define MAX_FILE_NAME_SIZE 128

char *__arthas_tracker_file_name(char *buf) {
  snprintf(buf, 128, "pmem_addr_pid_%d.dat", getpid());
  return buf;
}

void termination_handler(int signum) {
  fclose(__arthas_tracker_file);
  exit(-1);
}

void __arthas_addr_tracker_init() {
  char filename_buf[MAX_FILE_NAME_SIZE];
  char *filename = __arthas_tracker_file_name(filename_buf);
  fprintf(stderr, "openning address tracker output file %s\n", filename);
  __arthas_tracker_file = fopen(filename, "w");

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
  /*char full[20] = ",";
  char snum[20];
  sprintf(snum, "%d", (int)guid);
  strcat(full, snum);
  if(fscanf(__arthas_tracker_file, "%s", full) == 0)*/
    fprintf(__arthas_tracker_file, "%p,%u\n", addr, guid);

  //FIXME: Get rid of this eventually
  //fflush(__arthas_tracker_file);
}

bool __arthas_addr_tracker_dump() {
  fflush(__arthas_tracker_file);
  return true;
}

void __arthas_addr_tracker_finish() {
  // close the tracker file
  fclose(__arthas_tracker_file);
}
