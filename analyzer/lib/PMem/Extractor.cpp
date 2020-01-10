// The PMEM-Fault Project
//
// Created by ryanhuang on 12/24/19.
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Extractor.h"
#include "../DefUse/DefUse.h"

#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"

using namespace std;
using namespace llvm;
using namespace llvm::pmem;
using namespace llvm::defuse;

const set<std::string> PMemVariableLocator::pmdkApiSet{
    "pmem_map_file",         "pmem_persist", "pmem_msync",   "pmemobj_create",
    "pmemobj_direct_inline", "pmemobj_open", "pmem_map_file"};

const set<std::string> PMemVariableLocator::pmdkPMEMVariableReturnSet{
    "pmemobj_direct_inline", "pmem_map_file"};

const set<std::string> PMemVariableLocator::pmdkFileMappingSet{
    "pmemobj_create", "pmem_map_file"};

PMemVariableLocator::PMemVariableLocator(Function &F) {
  errs() << "[" << F.getName() << "]\n";
  for (inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ++I) {
    const Instruction *inst = &*I;
    if (!isa<CallInst>(inst)) continue;
    const CallInst *callInst = cast<CallInst>(inst);
    Function *callee = callInst->getCalledFunction();
    if (!callee) continue;
    // Step 1: Check for PMDK API call instructions
    if (pmdkApiSet.find(callee->getName()) != pmdkApiSet.end()) {
      callList.push_back(callInst);
      if (pmdkPMEMVariableReturnSet.find(callee->getName()) !=
          pmdkPMEMVariableReturnSet.end()) {
        // Step 2: if this API call returns something, we get a pmem variable.
        const Value *v = inst;
        errs() << "- this instruction creates a pmem variable: " << *v << "\n";
        varList.push_back(v);
        // Step 3: find the transitive closure of the users of the pmem variables.
        UserGraph g(v);
        for (auto ui = g.begin(); ui != g.end(); ++ui) {
          errs() << "- users of the pmem variable: " << *(ui->first) << "\n";
          varList.push_back(ui->first);
        }
      }
      // TODO: Step 4: Use alias analysis to find persistent pointers that point
      // to persistent variables
      //
      // Step 5: Use pmemobj_create calls to find persistent memory regions to
      // check all pointers if they point to a PMEM region.
      if (pmdkFileMappingSet.find(callee->getName()) !=
          pmdkFileMappingSet.end()) {
        int arg_count = 0;
        for (auto arg = callee->arg_begin(); arg != callee->arg_end(); ++arg) {
          if (arg_count == 2) {
            const Value *v = inst;
            const Value *v2 = &*arg;
            rangeList.insert(RangePair(v, v2));
          }
          arg_count++;
        }
      }
    }
  }
}

#define DEBUG_TYPE "pmem"
STATISTIC(PMVcounter, "Total number of variables backed by persistent memory");

namespace {
  class PMemVariablePass: public ModulePass {
    public:
      static char ID; 

      PMemVariablePass() : ModulePass(ID) {}

      virtual bool runOnModule(Module &M) override;

      void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesAll();
      }
    private:
      bool runOnFunction(Function &F);
  };
}

char PMemVariablePass::ID = 0;
static RegisterPass<PMemVariablePass> X("pmem", "Pass that analyzes variables backed by persistent memory");

bool PMemVariablePass::runOnModule(Module &M) {
  bool modified = false;
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
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
  for (auto ri = locator.range_begin(); ri != locator.range_end(); ++ri) {
    errs() << "* Identified pmem region [" << *ri->first << "," << *ri->second << "]\n";
  }
  return false;
}
