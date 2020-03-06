// The Arthas Project
//
// Created by ryanhuang on 12/24/19.
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef __INSTRUMENT_PMEMADDRPASS_H_
#define __INSTRUMENT_PMEMADDRPASS_H_

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

#include "Instrument/PmemAddrInstrumenter.h"

namespace llvm {

class InstrumentPmemAddrPass: public ModulePass {
 public:
  static char ID;
  InstrumentPmemAddrPass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

 protected:
  bool runOnFunction(Function &F);

 protected:
  std::unique_ptr<instrument::PmemAddrInstrumenter> _instrumenter;
};

} // end of namespace llvm

#endif /* __INSTRUMENT_PMEMADDRPASS_H_ */
