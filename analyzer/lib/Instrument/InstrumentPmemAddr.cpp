// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Instrument/InstrumentPmemAddr.h"

using namespace llvm;
using namespace llvm::pmem;
using namespace llvm::instrument;

bool PmemAddrInstrumenter::runOnModule(Module &M) 
{
  AddrHookFunction = M.getFunction("printf");
  if(!AddrHookFunction)
    errs() << "could not find printf\n";
  else
    errs() << "found printf\n";

  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    runOnFunction(F);
  }
  return false;
}

bool PmemAddrInstrumenter::runOnFunction(Function &F) 
{
  Instruction * instr;
  context = &F.getContext();
  for(inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ++ii){
    instr = &*ii;
    //TODO: modify to only include instructions in Slices, get locator to get Pmem variables
    //get slicer, find overlap in slices, pmem variables
    //pmem::PMemVariableLocator locator(F);
    instrument(instr);
  }
  return false; 
}

bool PmemAddrInstrumenter::instrument(Instruction *instr) {
  Value *addr;
  bool pool = false;

  if (isa<LoadInst>(instr)) {
    LoadInst *li = dyn_cast<LoadInst>(instr);
    addr = li->getPointerOperand();
  } else if (isa<StoreInst>(instr)) {
    StoreInst *si = dyn_cast<StoreInst>(instr);
    addr = si->getPointerOperand();
  } else if(isa<CallInst>(instr)){
    CallInst *ci = dyn_cast<CallInst>(instr);
    Function *callee = ci->getCalledFunction();
    if(callee->getName().compare("pmemobj_create") == 0){
      pool = true;
    }
    else {
      return false;
    }
  } else {
    return false;
  }

  // TODO: insert a call instruction to the hook function with CallInst::Create.
  // Pass addr as an argument to this call instruction
  IRBuilder <> builder(*context);
  builder.SetInsertPoint(instr->getNextNode());

  Value *str;
  if(pool){
    str = builder.CreateGlobalStringPtr("POOL address: %p\n");
  }else{
    str = builder.CreateGlobalStringPtr("address: %p\n");
  }
  //Value *str = builder.CreateGlobalStringPtr("address: %p\n");
  std::vector<llvm::Value*> params;
  params.push_back(str);
  
  params.push_back(addr);
  CallInst * print_call = CallInst::Create(AddrHookFunction, params, "call", instr->getNextNode());

  return true;
}

char PmemAddrInstrumenter::ID = 0;
static RegisterPass<PmemAddrInstrumenter> X("instr", "Instruments the code");
