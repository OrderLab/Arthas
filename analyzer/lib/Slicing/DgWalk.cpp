// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Slicing/DgWalk.h"

#include "dg/llvm/LLVMDependenceGraph.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;
using namespace dg::analysis;

uint32_t DgWalkBase::sliceRelationOpts(SliceDirection dir, bool full_relation)
{
  switch (dir) {
    case SliceDirection::Backward:
      if (full_relation) {
        return legacy::NODES_WALK_REV_CD | legacy::NODES_WALK_REV_DD |
          legacy::NODES_WALK_USER | legacy::NODES_WALK_ID |
          legacy::NODES_WALK_REV_ID;
      } else {
        // for simple relation, we only need data and user dependencies
        return legacy::NODES_WALK_REV_DD | legacy::NODES_WALK_USER;
      }
    case SliceDirection::Forward:
      if (full_relation) {
        return legacy::NODES_WALK_CD | legacy::NODES_WALK_DD |
               legacy::NODES_WALK_USE | legacy::NODES_WALK_ID;
      } else {
        // for simple relation, we only need data and use dependencies
        return legacy::NODES_WALK_DD | legacy::NODES_WALK_USE;
      }
    case SliceDirection::Full:
      // for full slicing, the bi-directional relationships need to be walked
      if (full_relation) {
        return legacy::NODES_WALK_CD | legacy::NODES_WALK_DD |
          legacy::NODES_WALK_USE | legacy::NODES_WALK_ID |
          legacy::NODES_WALK_REV_CD | legacy::NODES_WALK_REV_DD |
          legacy::NODES_WALK_USER | legacy::NODES_WALK_ID |
          legacy::NODES_WALK_REV_ID;
      } else {
        return legacy::NODES_WALK_DD | legacy::NODES_WALK_USE | 
          legacy::NODES_WALK_REV_DD | legacy::NODES_WALK_USER;
      }
    default:
      return 0;
  }
}

void DgWalkAndMark::mark(const std::set<dg::LLVMNode *> &start,
                         uint32_t slice_id) {
  DgWalkData data(slice_id, this, (_dir != SliceDirection::Backward) 
      ? &_markedBlocks : nullptr);
  this->walk(start, markSlice, &data);
}
void DgWalkAndMark::mark(dg::LLVMNode *start, uint32_t slice_id)
{
  DgWalkData data(slice_id, this, (_dir != SliceDirection::Backward) 
      ? &_markedBlocks : nullptr);
  this->walk(start, markSlice, &data);
}

void DgWalkAndMark::markSlice(dg::LLVMNode *n, DgWalkData *data)
{
  uint32_t slice_id = data->slice_id;
  n->setSlice(slice_id);
  errs() << "setting node " << *n->getValue() << " to slice " << slice_id << "\n";

  // NOTE: the original dg slicer marks the BB of a node, for us it should be
  // fine to skip this step.

  return;

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
      data->analysis->enqueue(entry);
    }
  }
}
