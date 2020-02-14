// The PMEM-Fault Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//
// A set of string related functions

#include <stdio.h>
#include <stdlib.h>

#include "Utils/String.h"

void *xmalloc(size_t size) {
  if (size <= 0) {
    fprintf(stderr, "Invalid malloc size %lu", size);
    exit(1);
  }
  void *p = malloc(size);
  if (p == NULL) {
    fprintf(stderr, "Out of memory");
    exit(1);
  }
  return p;
}

