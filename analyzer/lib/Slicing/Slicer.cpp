// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <chrono>
#include <ctime>
#include <set>
#include <utility>

#include <iostream>
#include <fstream>

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "DefUse/DefUse.h"
#include "Slicing/Slicer.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::defuse;

bool DgSlicer::compute() {
  dg::debug::TimeMeasure tm;
  tm.start();
  dg::analysis::pta::PointerAnalysis *pa;
  dg::LLVMPointerAnalysis pta(module);
  // create a flow-sensitive pointer analysis
  pa = pta.createPTA<dg::analysis::pta::PointerAnalysisFS>();
  pa->run();
  tm.stop();
  tm.report("INFO: points-to analysis took");
  const auto &nodes = pta.getNodes();

  errs() << "Points-to graph size " << nodes.size() << "\n";

  dg::llvmdg::LLVMDependenceGraphOptions dg_options;
  dg::llvmdg::LLVMPointerAnalysisOptions pta_options;
  // flow-sensitive
  pta_options.analysisType =
      dg::llvmdg::LLVMPointerAnalysisOptions::AnalysisType::fs;
  dg_options.PTAOptions = pta_options;

  builder = make_unique<dg::llvmdg::LLVMDependenceGraphBuilder>(module, dg_options);

  dg = std::move(builder->constructCFGOnly());
  if (!dg) {
    llvm::errs() << "Building the dependence graph failed!\n";
  }
  // compute both data dependencies (def-use) and control dependencies
  dg = builder->computeDependencies(std::move(dg));

  const auto &stats = builder->getStatistics();

  errs() << "[slicer] CPU time of pointer analysis: "
         << double(stats.ptaTime) / CLOCKS_PER_SEC << " s\n";
  errs() << "[slicer] CPU time of reaching definitions analysis: "
         << double(stats.rdaTime) / CLOCKS_PER_SEC << " s\n";
  errs() << "[slicer] CPU time of control dependence analysis: "
         << double(stats.cdTime) / CLOCKS_PER_SEC << " s\n";
  funcDgMap = &dg::getConstructedFunctions();

  return true;
}

dg::LLVMDependenceGraph *DgSlicer::getDependenceGraph(Function *func)
{
  if (funcDgMap == nullptr)
    return nullptr;
  auto dgit = funcDgMap->find(func);
  if (dgit == funcDgMap->end()) {
    errs() << "Could not find dependency graph for function " << func->getName() << "\n";
  }
  return dgit->second;
}
