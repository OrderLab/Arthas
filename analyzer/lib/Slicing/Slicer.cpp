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
  // first slice away bblocks that should go away
  sliceBBlocks(graph, slice_id);

  // NOTE: original LLVMSlicer will adjust the bbblocks successors.
  // For us, we don't really care if the graph if complete, so we skip it

  // now slice away instructions from BBlocks that left
  for (auto I = graph->begin(), E = graph->end(); I != E;) {
    dg::LLVMNode *n = I->second;
    // shift here, so that we won't corrupt the iterator
    // by deleteing the node
    ++I;

    // we added this node artificially and
    // we don't want to slice it away or
    // take any other action on it
    if (n == graph->getExit())
      continue;

    ++statistics.nodesTotal;

    // NOTE: the original LLVMSlicer keeps instructions like ret or unreachable
    // for us, we don't really care whether the slice is executable or not. 
    // so we assume it's OK to slice them away.

    if (n->getSlice() != slice_id) {
      removeNode(n);
      graph->deleteNode(n);
      ++statistics.nodesRemoved;
    }
  }
}

bool DgSlicer::removeBlock(dg::LLVMBBlock *block)
{
  assert(block);

  Value *val = block->getKey();
  if (val == nullptr)
    return true;

  BasicBlock *blk = cast<BasicBlock>(val);
  for (auto& succ : block->successors()) {
    if (succ.label == 255)
      continue;

    // don't adjust phi nodes in this block if this is a self-loop,
    // we're gonna remove the block anyway
    if (succ.target == block)
      continue;

    if (Value *sval = succ.target->getKey())
      dg::LLVMSlicer::adjustPhiNodes(cast<BasicBlock>(sval), blk);
  }

  // We need to drop the reference to this block in all
  // braching instructions that jump to this block.
  // See #99
  dg::dropAllUses(blk);

  // we also must drop refrences to instructions that are in
  // this block (or we would need to delete the blocks in
  // post-dominator order), see #101
  for (llvm::Instruction &Inst : *blk) 
    dg::dropAllUses(&Inst);

  // finally, erase the block per se
  blk->eraseFromParent();
  return true;
}

bool DgSlicer::removeNode(dg::LLVMNode *node)
{
  Value *val = node->getKey();
  // FIXME: may not be necessary for our purpose
  // if there are any other uses of this value,
  // just replace them with undef
  val->replaceAllUsesWith(UndefValue::get(val->getType()));

  Instruction *Inst = dyn_cast<Instruction>(val);
  if (Inst) {
    Inst->eraseFromParent();
  } else {
    GlobalVariable *GV = dyn_cast<GlobalVariable>(val);
    if (GV) GV->eraseFromParent();
  }
  return true;
}
