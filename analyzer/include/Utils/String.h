// The PMEM-Fault Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _UTILS_STRING_H_
#define _UTILS_STRING_H_

#include <string>
#include <vector>
#include <stddef.h>

void *xmalloc(size_t size);
int splitList(const std::string &str, char sep, std::vector<std::string> &ret);

#endif /* _UTILS_STRING_H_ */
