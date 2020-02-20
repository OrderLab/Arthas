// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef __INSTRUMENT_PMEMADDR_H_
#define __INSTRUMENT_PMEMADDR_H_

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Transforms/Instrumentation.h"

namespace llvm {

namespace instrument {

class PmemAddrInstrumenter {
 public:
  bool runOnFunction(Function &F);

  // instrument a call to hook func before an instruction.
  // this instruction must be a LoadInst or StoreInst
  bool instrument(Instruction * instr);

 protected:
  Function *AddrHookFunction;
  LLVMContext *context;
};

} // namespace llvm
} // namespace instrument

#endif /* __INSTRUMENT_PMEMADDR_H_ */
