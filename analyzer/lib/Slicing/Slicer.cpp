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
using namespace dg::analysis;

uint32_t DgWalkAndMark::sliceRelationOpts(SliceDirection dir)
{
  switch (dir) {
    case SliceDirection::Backward:
      return legacy::NODES_WALK_REV_CD | legacy::NODES_WALK_REV_DD |
             legacy::NODES_WALK_USER | legacy::NODES_WALK_ID |
             legacy::NODES_WALK_REV_ID;
    case SliceDirection::Forward:
      return legacy::NODES_WALK_CD | legacy::NODES_WALK_DD |
             legacy::NODES_WALK_USE | legacy::NODES_WALK_ID;
    case SliceDirection::Full:
      // for full slicing, the bi-directional relationships need to be walked
      return legacy::NODES_WALK_CD | legacy::NODES_WALK_DD |
             legacy::NODES_WALK_USE | legacy::NODES_WALK_ID |
             legacy::NODES_WALK_REV_CD | legacy::NODES_WALK_REV_DD |
             legacy::NODES_WALK_USER | legacy::NODES_WALK_ID |
             legacy::NODES_WALK_REV_ID;
    default:
      return 0;
  }
}

void DgWalkAndMark::mark(const std::set<dg::LLVMNode *> &start,
                         uint32_t slice_id, llvm::slicing::SliceGraph *sg)
{
  DgWalkData data(slice_id, this, (_dir != SliceDirection::Backward) 
      ? &_markedBlocks : nullptr);
  // FIXME: override enqueue, prepare and func to store the slice into sg
  this->walk(start, markSlice, &data);
}

void DgWalkAndMark::markSlice(dg::LLVMNode *n, DgWalkData *data)
{
  uint32_t slice_id = data->slice_id;
  n->setSlice(slice_id);
  //llvm::errs() << "mark slice of node " << n << "\n";

  // when we marked a node, we need to mark even
  // the basic block - if there are basic blocks
  if (DgBasicBlock *B = n->getBBlock()) {
    B->setSlice(slice_id);
    if (data->markedBlocks)
      data->markedBlocks->insert(B);
  }

  // the same with dependence graph, if we keep a node from
  // a dependence graph, we need to keep the dependence graph
  if (dg::LLVMDependenceGraph *dg = n->getDG()) {
    dg->setSlice(slice_id);
    if (!data->analysis->isForward()) {
      // and keep also all call-sites of this func (they are
      // control dependent on the entry node)
      // This is correct but not so precise - fix it later.
      // Now I need the correctness...
      dg::LLVMNode *entry = dg->getEntry();
      assert(entry && "No entry node in dg");
      //llvm::errs() << "this is the problem\n";
      data->analysis->enqueue(entry);
    }
  }
}

bool DgSlicer::computeDependencies()
{
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

uint32_t DgSlicer::slice(dg::LLVMNode *start, SliceGraph *sg, uint32_t sl_id) 
{
  assert(start || sl_id != 0);
  if (start) {
    // include argument for forward_slice here
    // FIXME: use DgWalkAndMark to store slice into sg
    sl_id = mark(start, sl_id, _direction == SliceDirection::Forward);
  }
  for (auto &it : *_funcDgMap) {
    dg::LLVMDependenceGraph *subdg = it.second;
    sliceGraph(subdg, sl_id);
  }
  return sl_id;
}

void DgSlicer::sliceGraph(dg::LLVMDependenceGraph *graph, uint32_t slice_id)
{
  // first slice away bblocks that should go away
  sliceBBlocks(graph, slice_id);

  // TODO: original LLVMSlicer will adjust the bbblocks successors.
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

    // TODO: the original LLVMSlicer keeps instructions like ret or unreachable
    // for us, we don't really care whether the slice is executable or not. 
    // so we assume it's OK to slice them away.

    /*
       if (llvm::isa<llvm::CallInst>(n->getKey()))
       sliceCallNode(n, slice_id);
       */

    if (n->getSlice() != slice_id) {
      removeNode(n);
      graph->deleteNode(n);
      ++statistics.nodesRemoved;
      //llvm::errs() << "nodes removed in slice " << statistics.nodesRemoved << "node " <<   n << "\n";
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
