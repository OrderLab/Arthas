// The Arthas Project
//
// Created by ryanhuang on 12/24/19.
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef __PMEMVARIABLEPASS_H_
#define __PMEMVARIABLEPASS_H_

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {

class PMemVariablePass : public ModulePass {
 public:
  static char ID;
  PMemVariablePass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

 private:
  bool runOnFunction(Function &F);
};

} // end of namespace llvm


#endif /* __PMEMVARIABLEPASS_H_ */
