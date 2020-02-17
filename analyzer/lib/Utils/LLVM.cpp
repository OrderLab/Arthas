// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//
// LLVM related functions

#include "Utils/LLVM.h"

using namespace std;
using namespace llvm;

unique_ptr<Module> parseModule(LLVMContext& context, string inputFile)
{
  SMDiagnostic SMD;

#if ((LLVM_VERSION_MAJOR == 3) && (LLVM_VERSION_MINOR <= 5))
  auto _M = ParseIRFile(inputFile, SMD, context);
  auto M = unique_ptr<Module>(_M);
#else
  auto M = parseIRFile(inputFile, SMD, context);
#endif

  if (!M) {
    SMD.print("llvm utils", errs());
  }
  return M;
}
