// The Arthas Project
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

int splitList(const string &str, const char *sep, vector<string> &result) {
  if (str.empty()) return 0;
  int n = 0;
  size_t start = 0;
  size_t end = str.find(sep);
  size_t sep_len = strlen(sep);
  while (end != std::string::npos) {
    result.push_back(str.substr(start, end - start));
    start = end + sep_len;
    end = str.find(sep, start);
    n++;
  }
  result.push_back(str.substr(start, end));
  return n;
}

bool split_untiln(const string &str, const char *delimeters, int n,
                  vector<string> &result, size_t *last_pos) {
  size_t prev_pos = str.find_first_not_of(delimeters);
  size_t pos = str.find_first_of(delimeters, prev_pos);
  int i = 1;
  while (prev_pos != string::npos || pos != string::npos) {
    result.push_back(str.substr(prev_pos, pos - prev_pos));
    if (i >= n) break;
    prev_pos = str.find_first_not_of(delimeters, pos);
    pos = str.find_first_of(delimeters, prev_pos);
    i++;
  }
  if (last_pos != NULL) {
    *last_pos = pos;
  }
  return i == n;
}
