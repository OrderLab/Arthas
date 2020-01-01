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
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
using namespace std;
using namespace llvm;
using namespace llvm::pmem;

const set<std::string> PMemAPICallLocator::pmdkApiSet {
  "pmem_map_file", "pmem_persist", "pmem_msync", "pmemobj_create", "pmemobj_direct_inline", "pmemobj_open", "pmem_map_file"
  };

const set<std::string> PMemAPICallLocator::pmdkPMEMVariableReturnSet {
  "pmemobj_direct_inline", "pmem_map_file"
  };

const set<std::string> PMemAPICallLocator::PMEMFileMappingSet{
  "pmemobj_create", "pmem_map_file"
  };

PMemAPICallLocator::PMemAPICallLocator(Function &F) {
  for (inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ++I) {
    const Instruction *inst = &*I;
    errs() << *I << "\n";
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
    //Step 1: Check for persistent variable by using pmemobj_direct calls
    if (pmdkApiSet.find(callee->getName()) != pmdkApiSet.end()) {
      callList.push_back(callInst);
      if (pmdkPMEMVariableReturnSet.find(callee->getName()) != pmdkPMEMVariableReturnSet.end()) {
	//Value *v = cast<Value>inst;
	const Value *v = inst;
	errs() << v->getName() << " end\n";
	candidateSet.push_back(v);
	for(auto U : v->users()){
	    if(auto I = dyn_cast<Instruction> (U)){
		errs() << "This Instruction uses a pmem variable:  " << *I << "\n";
		errs() << "Type is : " << I->getType()->getTypeID() << "\n";
		//One more level of indirection
		const Value *v2 = I;
		for(auto U2: v2->users()){
		   if(auto I2 = dyn_cast<Instruction> (U2)){
			errs() << "This Instruction uses a pmem variable:  " << *I2 << "\n";		   	
			if(auto S = dyn_cast<StoreInst> (I2)){
			   const Value *val = S->getPointerOperand();
			   errs() << "PERSISTENT VARIABLE IS: " << *val << "\n";
			   /*for(auto op = S->op_begin(); op != S->op_end(); op++){
			      Value *val = op->get();
			      //errs() << *val << "\n";
			      //StringRef name = val->getName(); 
			      //errs() << name.data() << "\n";
			   }*/
			}
		   }
		}
	    }
	}
      }
      //TODO: Step 2: Use alias analysis to find persistent pointers that point to persistent variables
      //Step 3: Use pmemobj_create calls to find persistent memory regions to check all pointers if they point to a PMEM region.
      if (PMEMFileMappingSet.find(callee->getName()) != PMEMFileMappingSet.end()) {
	int arg_count = 0;
	for(auto arg = callee->arg_begin(); arg != callee->arg_end(); ++arg){
		if(arg_count == 2){
			const Value *v = inst;
			const Value *v2 = &*arg;
			pmemRanges.insert(pair <const Value * , const Value *> (v, v2));
		}
		arg_count++;
	}
	errs() << "pmemobj_create \n";
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
