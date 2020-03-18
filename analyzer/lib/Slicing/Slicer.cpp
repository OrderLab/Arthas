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

#include "dg/analysis/Offset.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::defuse;

dg::llvmdg::LLVMDependenceGraphOptions DgSlicer::createDgOptions(
    bool intraprocedural, llvm::Function *entry) {
  // dependency graph options
  dg::llvmdg::LLVMDependenceGraphOptions dg_options;
  dg_options.intraprocedural = intraprocedural;
  dg_options.entryFunction = entry;
  dg_options.PTAOptions.intraprocedural = intraprocedural;
  dg_options.RDAOptions.intraprocedural = intraprocedural;
  dg_options.PTAOptions.entryFunction = entry;
  dg_options.RDAOptions.entryFunction = entry;
  // use flow-sensitive pointer analysis
  dg_options.PTAOptions.analysisType =
      dg::llvmdg::LLVMPointerAnalysisOptions::AnalysisType::fs;
  // use data-flow reaching definition analysis, another option is memory-ssa
  dg_options.RDAOptions.analysisType = dg::llvmdg::
      LLVMReachingDefinitionsAnalysisOptions::AnalysisType::dataflow;
  return dg_options;
}

bool DgSlicer::computeDependencies(
    dg::llvmdg::LLVMDependenceGraphOptions &options) {
  if (_dependency_computed) {
    return true;
  }

  _builder =
      make_unique<dg::llvmdg::LLVMDependenceGraphBuilder>(_module, options);

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

SliceGraph *DgSlicer::buildSliceGraph(dg::LLVMNode *start, uint32_t slice_id)
{
  DgWalkAndBuildSliceGraph wb(_direction, false);
  return wb.build(start, slice_id);
}

SliceGraph *DgSlicer::slice(dg::LLVMNode *start, uint32_t &slice_id,
                            SlicingApproachKind kind) 
{
  // If a slice id is not supplied, we use the last_slice_id + 1 as the new id
  if (slice_id == 0) slice_id = ++_last_slice_id;

  SliceGraph *result = nullptr;
  switch (kind) {
    case SlicingApproachKind::Storing:
      result = buildSliceGraph(start, slice_id);
      break;
    case SlicingApproachKind::Marking:
      markSliceId(start, slice_id);
      break;
    default:
      return nullptr;
  }
  for (auto &it : *_funcDgMap) {
    dg::LLVMDependenceGraph *subdg = it.second;
    updateStatsSliceId(subdg, slice_id);
  }
  return result;
}

SliceGraph *DgSlicer::slice(llvm::Instruction *start, uint32_t &slice_id,
                            SlicingApproachKind kind)
{
  Function *F = start->getFunction();
  dg::LLVMDependenceGraph *subdg = getDependenceGraph(F);
  if (subdg == nullptr) {
    errs() << "Failed to find dependence graph for " << F->getName() << "\n";
    return nullptr;
  }
  errs() << "Got dependence graph for function " << F->getName() << "\n";
  dg::LLVMNode *node = subdg->findNode(start);
  if (node == nullptr) {
    errs() << "Failed to find LLVMNode for " << *start << ", cannot slice\n";
    return nullptr;
  }
  errs() << "Computing slice for fault instruction " << *start << "\n";
  return slice(node, slice_id, kind);
}

void DgSlicer::updateStatsSliceId(dg::LLVMDependenceGraph *graph,
                                  uint32_t slice_id) {
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
    ++_statistics.blocksTotal;
    // if an entire basic block is not marked slice id, it's not in the slice
    if (blk->getSlice() != slice_id) {
      ++_statistics.blocksRemoved;
    }
  }
  ++_statistics.funcsTotal;
  if (graph->getSlice() != slice_id) {
    ++_statistics.funcsRemoved;
  }
}

void DgSlicer::walkSliceId(uint32_t slice_id, dg::LLVMDependenceGraph *graph,
    NodeSliceFunc nodeFunc, BasicBlockSliceFunc bbFunc, FunctionSliceFunc fnFunc)
{
  if (graph) {
    if (nodeFunc)
      walkNodeSliceId(graph, slice_id, nodeFunc);
    if (bbFunc)
      walkBasicBlockSliceId(graph, slice_id, bbFunc);
    if (fnFunc)
      walkFunctionSliceId(graph, slice_id, fnFunc);
  } else {
    for (auto &it : *_funcDgMap) {
      dg::LLVMDependenceGraph *subdg = it.second;
      if (nodeFunc)
        walkNodeSliceId(subdg, slice_id, nodeFunc);
      if (bbFunc)
        walkBasicBlockSliceId(subdg, slice_id, bbFunc);
      if (fnFunc)
        walkFunctionSliceId(subdg, slice_id, fnFunc);
    }
  }
}

void DgSlicer::walkNodeSliceId(dg::LLVMDependenceGraph *graph,
    uint32_t slice_id, NodeSliceFunc func)
{
  for (auto I = graph->begin(), E = graph->end(); I != E; ++I) {
    dg::LLVMNode *node = I->second;
    if (node == graph->getExit()) continue;
    if (node->getSlice() == slice_id) {
      func(node);
    }
  }
}

void DgSlicer::walkBasicBlockSliceId(dg::LLVMDependenceGraph *graph, 
        uint32_t slice_id, BasicBlockSliceFunc func)
{
  auto &blocks = graph->getBlocks();
  for (auto& it : blocks) {
    auto blk = it.second;
    if (blk->getSlice() == slice_id) {
      func(blk);
    }
  }
}

void DgSlicer::walkFunctionSliceId(dg::LLVMDependenceGraph *graph,
                                   uint32_t slice_id, FunctionSliceFunc func)
{
  if (graph->getSlice() == slice_id) {
    func(graph);
  }
}
