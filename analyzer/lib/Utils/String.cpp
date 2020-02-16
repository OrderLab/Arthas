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

using namespace std;

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

int splitList(const string& str, char sep, vector<string> &ret)
{
  if (str.empty())
    return 0;

  size_t prev_pos = 0;
  size_t pos = 0;
  int n = 0;
  while (true) {
    prev_pos = pos;
    pos = str.find(sep, pos);
    ret.push_back(str.substr(prev_pos, pos-prev_pos));
    n++;
    if (pos == std::string::npos)
      break;
    else
      ++pos;
  }
  return n;
}
