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

bool PmemAddrInstrumenter::runOnFunction(Function &F) 
{
  context = &F.getContext();
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
  return true;
}
