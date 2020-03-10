// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _REACTOR_OPTS_H_
#define _REACTOR_OPTS_H_

#include <string>

typedef struct reactor_options {
  /* argument options */
  const char *address_file;
  const char *checkpoint_file;
  const char *pmem_file;
  const char *pmem_layout;
  const char *pmem_library;
  const char *hook_guid_file;
  const char *reexecute_cmd;
  int version_num;

  // string representation of the fault instruction
  std::string fault_instr;
  // fault instruction location information
  std::string fault_loc;
  // the path to the bitcode file
  std::string bc_file;
} reactor_options;

extern const char *program;

void usage();
bool parse_options(int argc, char *argv[], reactor_options &options);
bool check_options(reactor_options &options);

#endif /* _REACTOR_OPTS_H_ */
