// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <iostream>

#include <llvm/Support/CommandLine.h>

#include "PMem/Extractor.h"
#include "PMem/PMemVariablePass.h"
#include "Utils/LLVM.h"
#include "Utils/String.h"

using namespace std;
using namespace llvm;
using namespace llvm::pmem;

cl::list<std::string> TargetFunctions("function", cl::desc("<Function>"),
                                      cl::ZeroOrMore);
cl::opt<string> inputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);

void extractPmemVarInFunc(Function *F)
{
  PMemVariableLocator locator;
  locator.runOnFunction(*F);

  // Iterate through the identified PMDK API calls in this function
  for (auto ci = locator.call_begin(); ci != locator.call_end(); ++ci) {
    errs() << "* Identified pmdk API calls: ";
    const CallInst *inst = *ci;
    inst->dump();
  }

  // Iterate through the identified PMem variables in this function
  for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
    errs() << "* Identified pmem variable instruction: " << **vi << "\n";
  }

  // Iterate through the identified PMem ranges in this function
  for (auto ri = locator.region_begin(); ri != locator.region_end(); ++ri) {
    errs() << "* Identified pmem region [" << *ri->first << "," << *ri->second << "]\n";
  }
}

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv);

  LLVMContext context;
  unique_ptr<Module> M = parseModule(context, inputFilename);
  if (!M) {
    errs() << "Failed to parse '" << inputFilename << "' file:\n";
    return 1;
  }
  set<string> targetFunctionSet(TargetFunctions.begin(), TargetFunctions.end());
  for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
      if (targetFunctionSet.empty() ||
          targetFunctionSet.count(F.getName()) != 0)
        extractPmemVarInFunc(&F);
    }
  }
}
