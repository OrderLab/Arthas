// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Instrument/InstrumentPmemAddr.h"

using namespace llvm;
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
  verifyModule(M);
  outs() << M;
  return false;
}

bool PmemAddrInstrumenter::runOnFunction(Function &F) 
{
  Instruction * instr;
  context = &F.getContext();
  for(inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ++ii){
    instr = &*ii;
    //TODO: modify to only include instructions in Slices.
    instrument(instr);
  }
  return false; 
}

bool PmemAddrInstrumenter::instrument(Instruction *instr) 
{
  Value *addr;
  if (isa<LoadInst>(instr)) {
    LoadInst *li = dyn_cast<LoadInst>(instr);
    addr = li->getPointerOperand();
  } else if (isa<StoreInst>(instr)) {
    StoreInst *si = dyn_cast<StoreInst>(instr);
    addr = si->getPointerOperand();
  } else {
    return false;
  }
  // TODO: insert a call instruction to the hook function with CallInst::Create.
  // Pass addr as an argument to this call instruction
  IRBuilder <> builder(*context);
  builder.SetInsertPoint(instr->getNextNode());

  Value *str = builder.CreateGlobalStringPtr("address: %p\n");
  std::vector<llvm::Value*> params;
  params.push_back(str);
   
  params.push_back(addr);
  CallInst * print_call = CallInst::Create(AddrHookFunction, params, "call", instr->getNextNode());
  return true;
}

char PmemAddrInstrumenter::ID = 0;
static RegisterPass<PmemAddrInstrumenter> X("instr", "Instruments the code");
