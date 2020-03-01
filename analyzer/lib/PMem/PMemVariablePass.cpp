// The Arthas Project
//
// Created by ryanhuang on 12/24/19.
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "PMem/PMemVariablePass.h"
#include "PMem/Extractor.h"

#include "llvm/Support/CommandLine.h"

using namespace std;
using namespace llvm;
using namespace llvm::pmem;

// opt argument to extract persistent variables only in the target functions
static cl::list<std::string> TargetFunctions("target-functions",
                                      cl::desc("<Function>"), cl::ZeroOrMore);

char PMemVariablePass::ID = 0;
static RegisterPass<PMemVariablePass> X("pmem", "Pass that analyzes variables backed by persistent memory");

bool PMemVariablePass::runOnModule(Module &M) {
  set<string> targetFunctionSet(TargetFunctions.begin(), TargetFunctions.end());
  bool modified = false;
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
      if (targetFunctionSet.empty() ||
          targetFunctionSet.count(F.getName()) != 0)
        modified |= runOnFunction(F);
    }
  }
  return modified;
}

bool PMemVariablePass::runOnFunction(Function &F) {
  PMemVariableLocator locator(F);

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
  return false;
}
