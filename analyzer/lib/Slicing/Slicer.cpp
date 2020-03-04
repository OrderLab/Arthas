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
#include "Slicing/DgWalk.h"
#include "Slicing/Slicer.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::defuse;

bool DgSlicer::computeDependencies() {
  if (_dependency_computed) {
    return true;
  }
  // dependency graph options
  dg::llvmdg::LLVMDependenceGraphOptions dg_options;
  // use flow-sensitive pointer analysis
  dg_options.PTAOptions.analysisType =
      dg::llvmdg::LLVMPointerAnalysisOptions::AnalysisType::fs;
  // use data-flow reaching definition analysis, another option is memory-ssa
  dg_options.RDAOptions.analysisType = dg::llvmdg::
      LLVMReachingDefinitionsAnalysisOptions::AnalysisType::dataflow;

  _builder = make_unique<dg::llvmdg::LLVMDependenceGraphBuilder>(_module, dg_options);

  _dg = std::move(_builder->constructCFGOnly());
  if (!_dg) {
    llvm::errs() << "Building the dependence graph failed!\n";
    return false;
  }
  // compute both data dependencies (def-use) and control dependencies
  _dg = _builder->computeDependencies(std::move(_dg));
  _dg->verify();

  const auto &stats = _builder->getStatistics();
  errs() << "[slicer] CPU time of pointer analysis: "
         << double(stats.ptaTime) / CLOCKS_PER_SEC << " s\n";
  errs() << "[slicer] CPU time of reaching definitions analysis: "
         << double(stats.rdaTime) / CLOCKS_PER_SEC << " s\n";
  errs() << "[slicer] CPU time of control dependence analysis: "
         << double(stats.cdTime) / CLOCKS_PER_SEC << " s\n";
  _funcDgMap = &dg::getConstructedFunctions();
  _dependency_computed = true;
  return true;
}

dg::LLVMDependenceGraph *DgSlicer::getDependenceGraph(Function *func)
{
  if (_funcDgMap == nullptr)
    return nullptr;
  auto dgit = _funcDgMap->find(func);
  if (dgit == _funcDgMap->end()) {
    errs() << "Could not find dependency graph for function " << func->getName() << "\n";
  }
  return dgit->second;
}

uint32_t DgSlicer::markSliceId(dg::LLVMNode *start, uint32_t slice_id)
{
  // If a slice id is not supplied, we use the last_slice_id + 1 as the new id
  if (slice_id == 0) slice_id = ++_last_slice_id;

  // only walk the data and use dependencies
  DgWalkAndMark wm(_direction, false);
  wm.mark(start, slice_id);

  return slice_id;

  // If we are performing forward slicing, we are missing the control dependencies 
  // now. So gather all control dependencies of the nodes that we want to have in 
  // the slice and perform normal backward slicing w.r.t these nodes.
  if (_direction == SliceDirection::Forward) {
    std::set<dg::LLVMNode *> branchings;
    for (auto *BB : wm.getMarkedBlocks()) {
      for (auto cBB : BB->revControlDependence()) {
        assert(cBB->successorsNum() > 1);
        branchings.insert(cBB->getLastNode());
      }
    }
    if (!branchings.empty()) {
      DgWalkAndMark wm2(SliceDirection::Backward);
      wm2.mark(branchings, slice_id);
    }
  }
  return slice_id;
}

uint32_t DgSlicer::slice(dg::LLVMNode *start, SliceGraph *sg, uint32_t slice_id) 
{
  assert(start || slice_id != 0);
  if (start) {
    slice_id = markSliceId(start, slice_id);
  }
  for (auto &it : *_funcDgMap) {
    dg::LLVMDependenceGraph *subdg = it.second;
    sliceGraph(subdg, slice_id);
  }
  return slice_id;
}

void DgSlicer::sliceGraph(dg::LLVMDependenceGraph *graph, uint32_t slice_id)
{
  // NOTE: original dg slicer will do a bunch of fix-ups in the slice graph 
  // function, such as adjusting the bbblocks successors. Its purpose is
  // to make the sliced graph still executable. For us, we don't really
  // care whether the slicing result is executable. So we skip them and
  // mainly update the statistics.

  // We just update the statistics of sliced blocks and nodes. 
  // We don't actually remove nodes or basic blocks.
  for (auto I = graph->begin(), E = graph->end(); I != E; ++I) {
    dg::LLVMNode *node = I->second;
    if (node == graph->getExit()) continue;
    ++_statistics.nodesTotal;
    if (node->getSlice() != slice_id) {
      ++_statistics.nodesRemoved;
    }
  }
  auto &blocks = graph->getBlocks();
  for (auto& it : blocks) {
    auto blk = it.second;
    // if an entire basic block is not marked slice id, it's not in the slice
    if (blk->getSlice() != slice_id) {
      ++_statistics.blocksRemoved;
    }
  }
}
