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

typedef struct dg_options {
  // only analyzing the function that a fault instruction belongs to.
  bool entry_only;
  // enable pointer analysis
  bool enable_pta;
  // enable control will build control edges in dependence graph
  bool enable_ctrl;
  // slice control will include control edges in slices
  bool slice_ctrl;
  // support dependencies due to thread related operationssuch as pthread_create
  bool support_thread;
  // intra-procedural option analyzes other functions in the executable
  // but do not establish connections among functions
  bool intra_procedural;
  // intra-procedural option analyzes other functions in the executable
  // and establish connections among functions
  bool inter_procedural;
} dg_options_t;

typedef struct reactor_options {
  /* argument options */
  const char *address_file;
  const char *checkpoint_file;
  const char *pmem_file;
  const char *pmem_layout;
  const char *pmem_library;
  const char *hook_guid_file;
  const char *reexecute_cmd;
  bool arckpt;
<<<<<<< HEAD
  int batch_threshold;
=======
>>>>>>> 41f05f787706ac156000075aed00e8d010cc54e6
  int version_num;

  // string representation of the fault instruction
  std::string fault_instr;
  // fault instruction location information
  std::string fault_loc;
  // the path to the bitcode file
  std::string bc_file;

  dg_options_t dg_options;
} reactor_options_t;

extern const char *program;

void usage();
bool parse_options(int argc, char *argv[], reactor_options &options);
bool check_options(reactor_options &options);

#endif /* _REACTOR_OPTS_H_ */
