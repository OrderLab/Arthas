// The PMEM-Fault Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <utility>
#include <ctime>
#include <chrono>

#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "dg/llvm/LLVMDependenceGraph.h"
#include "dg/llvm/LLVMDependenceGraphBuilder.h"
#include "dg/llvm/LLVMNode.h"
#include "dg/llvm/LLVMSlicer.h"
#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"

using namespace llvm;

namespace {
class Slicer : public ModulePass {
 public:
  static char ID;
  Slicer() : ModulePass(ID) {}

  bool runOnModule(Module &M) {
    dg::llvmdg::LLVMDependenceGraphBuilder builder(&M);
    dg::LLVMSlicer slicer;
    std::unique_ptr<dg::LLVMDependenceGraph> dg{};

    dg = std::move(builder.constructCFGOnly());
    if (!dg) {
      llvm::errs() << "Building the dependence graph failed!\n";
    }
    dg = builder.computeDependencies(std::move(dg));
    const auto &stats = builder.getStatistics();
    llvm::errs() << "[slicer] CPU time of pointer analysis: "
                 << double(stats.ptaTime) / CLOCKS_PER_SEC << " s\n";
    llvm::errs() << "[slicer] CPU time of reaching definitions analysis: "
                 << double(stats.rdaTime) / CLOCKS_PER_SEC << " s\n";
    llvm::errs() << "[slicer] CPU time of control dependence analysis: "
                 << double(stats.cdTime) / CLOCKS_PER_SEC << " s\n";
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }
};
}

char Slicer::ID = 0;
static RegisterPass<Slicer> X("slicer", "Slices the code");
