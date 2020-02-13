// The PMEM-Fault Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef __PATH_H_
#define __PATH_H_

const char *stripname(const char *name, int strips);
char *canonpath(const char *name, char *resolved);
bool pathendswith(const char *path1, const char *path2);
bool pathneq(const char *path1, const char *path2, int n);

#endif /* __PATH_H_ */
