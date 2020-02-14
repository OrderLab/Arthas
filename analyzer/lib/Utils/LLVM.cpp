// The PMEM-Fault Project
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
  auto _M = llvm::ParseIRFile(inputFile, SMD, context);
  auto M = std::unique_ptr<llvm::Module>(_M);
#else
  auto M = llvm::parseIRFile(inputFile, SMD, context);
#endif

  if (!M) {
    SMD.print("llvm utils", errs());
  }
  return M;
}
