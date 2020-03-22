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

uint32_t DgWalkBase::sliceRelationOpts(SliceDirection dir,
                                       uint32_t slice_dep_flags) {
  uint32_t flags = 0;
  switch (dir) {
    case SliceDirection::Backward:
      if (slice_dep_flags & SliceDependenceFlags::DEF_USE)
        flags |= legacy::NODES_WALK_USER;
      if (slice_dep_flags & SliceDependenceFlags::MEMORY)
        flags |= legacy::NODES_WALK_REV_DD;
      if (slice_dep_flags & SliceDependenceFlags::CONTROL)
        flags |= legacy::NODES_WALK_REV_CD;
      if (slice_dep_flags & SliceDependenceFlags::INTERFERENCE)
        flags |= legacy::NODES_WALK_REV_ID;
      break;
    case SliceDirection::Forward:
      if (slice_dep_flags & SliceDependenceFlags::DEF_USE)
        flags |= legacy::NODES_WALK_USE;
      if (slice_dep_flags & SliceDependenceFlags::MEMORY)
        flags |= legacy::NODES_WALK_DD;
      if (slice_dep_flags & SliceDependenceFlags::CONTROL)
        flags |= legacy::NODES_WALK_CD;
      if (slice_dep_flags & SliceDependenceFlags::INTERFERENCE)
        flags |= legacy::NODES_WALK_ID;
      break;
    case SliceDirection::Full:
      if (slice_dep_flags & SliceDependenceFlags::DEF_USE)
        flags |= (legacy::NODES_WALK_USE | legacy::NODES_WALK_USER);
      if (slice_dep_flags & SliceDependenceFlags::MEMORY)
        flags |= (legacy::NODES_WALK_DD | legacy::NODES_WALK_REV_DD);
      if (slice_dep_flags & SliceDependenceFlags::CONTROL)
        flags |= (legacy::NODES_WALK_CD | legacy::NODES_WALK_REV_CD);
      if (slice_dep_flags & SliceDependenceFlags::INTERFERENCE)
        flags |= (legacy::NODES_WALK_ID | legacy::NODES_WALK_REV_ID);
      break;
  }
  return flags;
}

bool DgWalkBase::shouldSliceInst(const Instruction *inst) {
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
  Instruction *root_val = dyn_cast<Instruction>(start->getValue());
  if (!root_val) {
    return nullptr;
  }
  errs() << "Building a graph for slice " << slice_id << "\n";
  SliceNode *root = new SliceNode(root_val, 0);
  SliceGraph *sg = new SliceGraph(root, _dir, slice_id);
  // run_id is used to indicate whether a node has been visited or not
  // we should ensure it's unique by incrementing the global run counter
  run_id = ++walk_run_counter;
  enqueue(start);

  SliceNode *sn_curr;
  dg::LLVMNode *dn_curr;
  while (!queue.empty()) {
    dn_curr = queue.pop();
    _dfs_order++;
    if (options == 0) continue;
    Instruction *inst = dyn_cast<Instruction>(dn_curr->getValue());
    if (!inst || !shouldSliceInst(inst)) continue;

    // we also mark the node and bb with slice id for counting the statistics
    mark(dn_curr, slice_id);
    sn_curr = sg->getOrCreateNode(inst);
    if (options & legacy::NODES_WALK_USE) {
      if (sg->getDirection() == SliceDirection::Backward) {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::RegisterDefUse,
                          dn_curr->use_rbegin(), dn_curr->use_rend());
      } else {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::RegisterDefUse,
                          dn_curr->use_begin(), dn_curr->use_end());
      }
    }
    if (options & legacy::NODES_WALK_USER) {
      if (sg->getDirection() == SliceDirection::Backward) {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::RegisterDefUse,
                          dn_curr->user_rbegin(), dn_curr->user_rend());
      } else {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::RegisterDefUse,
                          dn_curr->user_begin(), dn_curr->user_end());
      }
    }
    if (options & legacy::NODES_WALK_DD) {
      if (sg->getDirection() == SliceDirection::Backward) {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::MemoryDependence,
                          dn_curr->data_rbegin(), dn_curr->data_rend());
      } else {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::MemoryDependence,
                          dn_curr->data_begin(), dn_curr->data_end());
      }
    }
    if (options & legacy::NODES_WALK_REV_DD) {
      if (sg->getDirection() == SliceDirection::Backward) {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::MemoryDependence,
                          dn_curr->rev_data_rbegin(), dn_curr->rev_data_rend());
      } else {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::MemoryDependence,
                          dn_curr->rev_data_begin(), dn_curr->rev_data_end());
      }
    }
    if (options & legacy::NODES_WALK_CD) {
      dg::LLVMBBlock *block = dn_curr->getBBlock();
      if (sg->getDirection() == SliceDirection::Backward) {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::ControlDependence,
                          dn_curr->control_rbegin(), dn_curr->control_rend());
        processControlBlocks(sg, sn_curr, block->control_rbegin(),
                             block->control_rend());
      } else {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::ControlDependence,
                          dn_curr->control_begin(), dn_curr->control_end());
        processControlBlocks(sg, sn_curr, block->control_begin(),
                             block->control_end());
      }
    }
    // Note: we need to properly handle control dependencies.
    //
    // The control dependencies of a LLVMNode is not really stored in the node's
    // control dependency list. Rather, it the control dependencies such as
    //      if (flag) {
    //        a = 10;
    //        b = 5;
    //      }
    // are stored at the basic block (LLVMBBlock) granularity in the dg.
    // This is reasonable as all instructions in the same basic block share
    // the same control dependencies. The implication is that when building
    // the slice graph, we need to re-establish the control dependencies
    // at the instruction granularity by creating an edge from the instruction
    // to the last node of the control-dependent basic block.
    //
    if (options & legacy::NODES_WALK_REV_CD) {
      // find the basic block of the current node
      dg::LLVMBBlock *block = dn_curr->getBBlock();
      if (sg->getDirection() == SliceDirection::Backward) {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::ControlDependence,
                          dn_curr->rev_control_rbegin(),
                          dn_curr->rev_control_rend());
        // establish control dependencies between the last nodes of the
        // control dependent blocks and the current node
        processControlBlocks(sg, sn_curr, block->rev_control_rbegin(),
                             block->rev_control_rend());
      } else {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::ControlDependence,
                          dn_curr->rev_control_begin(),
                          dn_curr->rev_control_end());
        processControlBlocks(sg, sn_curr, block->rev_control_begin(),
                             block->rev_control_end());
      }
    }
    if (options & legacy::NODES_WALK_ID) {
      if (sg->getDirection() == SliceDirection::Backward) {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::InterfereDependence,
                          dn_curr->interference_rbegin(),
                          dn_curr->interference_rend());
      } else {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::InterfereDependence,
                          dn_curr->interference_begin(),
                          dn_curr->interference_end());
      }
    }
    if (options & legacy::NODES_WALK_REV_ID) {
      if (sg->getDirection() == SliceDirection::Backward) {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::InterfereDependence,
                          dn_curr->rev_interference_rbegin(),
                          dn_curr->rev_interference_rend());
      } else {
        processSliceEdges(sg, sn_curr, SliceEdge::EdgeKind::InterfereDependence,
                          dn_curr->rev_interference_begin(),
                          dn_curr->rev_interference_end());
      }
    }
  }
  errs() << "Slice graph " << slice_id << " is constructed\n";
  return sg;
}
