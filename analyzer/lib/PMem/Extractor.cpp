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

using namespace std;
using namespace llvm;
using namespace llvm::pmem;

const set<std::string> PMemAPICallLocator::pmdkApiSet {
  "pmem_map_file", "pmem_persist", "pmem_msync", "pmemobj_create", "pmemobj_direct_inline"
  };

const set<std::string> PMemAPICallLocator::pmdkPMEMVariableReturnSet {
  "pmemobj_direct_inline"
  };

PMemAPICallLocator::PMemAPICallLocator(Function &F) {
  for (inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ++I) {
    const Instruction *inst = &*I;
    if (!isa<CallInst>(inst))
      continue;
    const CallInst *callInst = cast<CallInst>(inst);
    Function *callee = callInst->getCalledFunction();
    if (!callee)
      continue;
    errs() << "[";
    errs().write_escaped(F.getName()) << "]";
    errs() << " calling function ";
    errs().write_escaped(callee->getName()) << '\n';
    if (pmdkApiSet.find(callee->getName()) != pmdkApiSet.end()) {
      callList.push_back(callInst);
      if (pmdkPMEMVariableReturnSet.find(callee->getName()) != pmdkPMEMVariableReturnSet.end()) {
	//Value *v = cast<Value>inst;
	//errs() << v->getName() << " end\n";
	for(auto U : inst->users()){
	    if(auto I = dyn_cast<Instruction> (U)){
		errs() << "This Instruction uses a pmem variable:  " << *I << "\n";
	    }
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
  PMemAPICallLocator locator(F);
  // Iterate through the identified PMDK API calls in this function
  for (auto ci = locator.call_begin(); ci != locator.call_end(); ++ci) {
    errs() << " Identified pmdk API calls: ";
    const CallInst *inst = *ci;
    inst->dump();
  }
  // TODO: the next step is to use the call inst to identify the 
  // persistent variables
  return false;
}
