// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Slicing/SliceGraph.h"
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

bool DgWalkBase::shouldSliceInst(const llvm::Value *val) {
  const Instruction *inst = dyn_cast<Instruction>(val);
  if (!inst) return false;

  errs() << inst->getOpcodeName() << ":" << *inst << "\n";
  switch (inst->getOpcode()) {
    case Instruction::Unreachable:
      return false;
    case Instruction::Ret: {
      // FIXME: for now, we only ignore return instruction
      // if it returns void, probably should be more
      // aggressive to not include return instruction at all
      auto ret = cast<ReturnInst>(inst);
      return ret->getReturnValue() != nullptr;
    }
    default:
      return true;
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

  // when we marked a node, we need to mark even
  // the basic block - if there are basic blocks
  if (DgBasicBlock *B = n->getBBlock()) {
    B->setSlice(slice_id);
    if (data->markedBlocks) data->markedBlocks->insert(B);
  }

  // the same with dependence graph, if we keep a node from
  // a dependence graph, we need to keep the dependence graph
  if (dg::LLVMDependenceGraph *dg = n->getDG()) {
    dg->setSlice(slice_id);
  }
}

void DgWalkAndBuildSliceGraph::mark(dg::LLVMNode *n, uint32_t slice_id)
{
  n->setSlice(slice_id);
  if (DgBasicBlock *B = n->getBBlock()) {
    B->setSlice(slice_id);
  }
  if (dg::LLVMDependenceGraph *dg = n->getDG()) {
    dg->setSlice(slice_id);
  }
}

SliceGraph *DgWalkAndBuildSliceGraph::build(dg::LLVMNode *start,
                                            uint32_t slice_id) {
  errs() << "Building a graph for slice " << slice_id << "\n";
  SliceNode *root = new SliceNode(start->getValue(), 0);
  SliceGraph *sg = new SliceGraph(root, _dir, slice_id);
  // run_id is used to indicate whether a node has been visited or not
  // we should ensure it's unique by incrementing the global run counter
  run_id = ++walk_run_counter;
  enqueue(start);

  SliceNode *sn_curr;
  dg::LLVMNode *dn_curr;
  while (!queue.empty()) {
    dn_curr = queue.pop();
    if (options == 0) continue;
    if (!shouldSliceInst(dn_curr->getValue())) continue;

    // we also mark the node and bb with slice id for counting the statistics
    mark(dn_curr, slice_id);
    sn_curr = sg->getOrCreateNode(dn_curr->getValue());
    if (options & legacy::NODES_WALK_CD) {
      processSliceEdges(sg, sn_curr, dn_curr,
                        SliceEdge::EdgeKind::ControlDependence,
                        dn_curr->control_begin(), dn_curr->control_end());
    }
    if (options & legacy::NODES_WALK_REV_CD) {
      processSliceEdges(
          sg, sn_curr, dn_curr, SliceEdge::EdgeKind::ControlDependence,
          dn_curr->rev_control_begin(), dn_curr->rev_control_end());
    }
    if (options & legacy::NODES_WALK_DD) {
      processSliceEdges(sg, sn_curr, dn_curr,
                        SliceEdge::EdgeKind::MemoryDependence,
                        dn_curr->data_begin(), dn_curr->data_end());
    }
    if (options & legacy::NODES_WALK_REV_DD) {
      processSliceEdges(sg, sn_curr, dn_curr,
                        SliceEdge::EdgeKind::MemoryDependence,
                        dn_curr->rev_data_begin(), dn_curr->rev_data_end());
    }
    if (options & legacy::NODES_WALK_USE) {
      processSliceEdges(sg, sn_curr, dn_curr,
                        SliceEdge::EdgeKind::RegisterDefUse,
                        dn_curr->use_begin(), dn_curr->use_end());
    }
    if (options & legacy::NODES_WALK_USER) {
      processSliceEdges(sg, sn_curr, dn_curr,
                        SliceEdge::EdgeKind::RegisterDefUse,
                        dn_curr->user_begin(), dn_curr->user_end());
    }
    if (options & legacy::NODES_WALK_ID) {
      processSliceEdges(
          sg, sn_curr, dn_curr, SliceEdge::EdgeKind::InterfereDependence,
          dn_curr->interference_begin(), dn_curr->interference_end());
    }
    if (options & legacy::NODES_WALK_REV_ID) {
      processSliceEdges(
          sg, sn_curr, dn_curr, SliceEdge::EdgeKind::InterfereDependence,
          dn_curr->rev_interference_begin(), dn_curr->rev_interference_end());
    }
  }
  errs() << "Slice graph " << slice_id << " is constructed\n";
  return sg;
}
