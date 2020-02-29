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

char *__arthas_tracker_file_name(char * buf)
{
  snprintf(buf, 128, "pmem_addr_pid_%d.dat", getpid()); 
  return buf;
}

void __arthas_addr_tracker_init() 
{
  char filename_buf[MAX_FILE_NAME_SIZE];
  char *filename = __arthas_tracker_file_name(filename_buf);
  fprintf(stderr, "openning address tracker output file %s\n", filename);
  __arthas_tracker_file = fopen(filename, "w");
}

inline void __arthas_track_addr(char *addr, const char *scope, unsigned int line) 
{
  // TODO: replace the fprintf with buffering and async write
  fprintf(__arthas_tracker_file, "%p,%s,%d\n", addr, scope, line);
}

bool __arthas_addr_tracker_dump()
{
  fflush(__arthas_tracker_file);
  return true;
}

void __arthas_addr_tracker_finish()
{
  // close the tracker file
  fclose(__arthas_tracker_file);
}
