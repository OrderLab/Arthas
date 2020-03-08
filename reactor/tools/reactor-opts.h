// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _REACTOR_OPTS_H_
#define _REACTOR_OPTS_H_

typedef struct reactor_options {
  /* argument options */
  const char *address_file;
  const char *checkpoint_file;
  const char *pmem_file;
  const char *pmem_layout;
  const char *pmem_library;
  const char *hook_guid_file;
  int version_num;
} reactor_options;

extern const char *program;

void usage();
bool parse_options(int argc, char *argv[], reactor_options &options);
bool check_options(reactor_options &options);

#endif /* _REACTOR_OPTS_H_ */
