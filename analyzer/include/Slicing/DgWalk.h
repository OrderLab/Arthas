// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _SLICING_DGWALK_H_
#define _SLICING_DGWALK_H_

#include "Slicing/Slice.h"
#include "Slicing/SliceGraph.h"

#include "dg/analysis/legacy/Analysis.h"
#include "dg/analysis/legacy/BFS.h"
#include "dg/analysis/legacy/NodesWalk.h"
#include "dg/llvm/LLVMNode.h"

namespace llvm {
namespace slicing {

using DgDfsQueue = dg::ADT::QueueLIFO<dg::LLVMNode *>;
using DgBfsQueue = dg::ADT::QueueFIFO<dg::LLVMNode *>;

template <template <typename> class QueueT>
using DgNodesWalkT =
    dg::analysis::legacy::NodesWalk<dg::LLVMNode, QueueT<dg::LLVMNode *>>;

// DFS uses a stack (LIFO)
using DgDfsNodeWalkT = DgNodesWalkT<dg::ADT::QueueLIFO>;
// BFS uses a queue (FIFO)
using DgBfsNodeWalkT = DgNodesWalkT<dg::ADT::QueueFIFO>;

using DgBasicBlock = dg::BBlock<dg::LLVMNode>;

class DgWalkBase {
 public:
  DgWalkBase(SliceDirection dir) : _dir(dir) {}

  inline bool isForward() const { return _dir == SliceDirection::Forward; }
  inline bool isBackward() const { return _dir == SliceDirection::Backward; }
  inline bool isFull() const { return _dir == SliceDirection::Full; }

  static uint32_t sliceRelationOpts(SliceDirection dir, bool full_relation);
  static bool shouldSliceInst(const llvm::Instruction *inst);

 protected: 
  SliceDirection _dir;
};

class DgWalkDFS : public DgWalkBase, public DgDfsNodeWalkT {
 public:
  DgWalkDFS(SliceDirection dir, bool full_depdencies = false)
      : DgWalkBase(dir),
        DgDfsNodeWalkT(sliceRelationOpts(dir, full_depdencies)),
        _dfs_order(0){};

 protected:
  unsigned int _dfs_order;
};

class DgWalkBFS : public DgWalkBase, public DgBfsNodeWalkT {
 public:
  DgWalkBFS(SliceDirection dir, bool full_depdencies = false)
      : DgWalkBase(dir),
        DgBfsNodeWalkT(sliceRelationOpts(dir, full_depdencies)),
        _bfs_order(0){};

 protected:
  unsigned int _bfs_order;
};

// This class will go through the nodes will mark the ones that should be in the
// slice. For the marking approach, it does not really matter whether it's DFS
// or BFS marking. We just use BFS like the original WalkAndMark in dg.
class DgWalkAndMark : public DgWalkBFS {
 public:
  DgWalkAndMark(SliceDirection dir, bool full_depdencies = false)
      : DgWalkBFS(dir, full_depdencies) {}

  void mark(const std::set<dg::LLVMNode *> &start, uint32_t slice_id);
  void mark(dg::LLVMNode *start, uint32_t slice_id);

  const std::set<DgBasicBlock *>& getMarkedBlocks() { return _markedBlocks; }

 protected:
  std::set<DgBasicBlock *> _markedBlocks;

  struct DgWalkData {
    DgWalkData(uint32_t si, DgWalkAndMark *wm,
               std::set<DgBasicBlock *> *mb = nullptr)
        : slice_id(si), analysis(wm), markedBlocks(mb) {}
    uint32_t slice_id;
    DgWalkAndMark *analysis;
    std::set<DgBasicBlock *> *markedBlocks;
  };

  static void markSlice(dg::LLVMNode *n, DgWalkData *data);
};

// This class will walk the dependency graph. Different from the marking approach, 
// it will directly construct slices as it walks the graph, which can preserve
// the ordering of walking. The ordering of dependencies can be useful, e.g., to
// determine heuristics of which slice to use first.
class DgWalkAndBuildSliceGraph: public DgWalkDFS {
 public:
  DgWalkAndBuildSliceGraph(SliceDirection dir, bool full_depdencies = false)
      : DgWalkDFS(dir, full_depdencies) {}

  SliceGraph *build(dg::LLVMNode *start, uint32_t slice_id);

 protected:
  template <typename EdgeIterT>
  void processSliceEdges(SliceGraph *sg, SliceNode *sn_curr,
                         dg::LLVMNode *dn_curr, SliceEdge::EdgeKind kind,
                         EdgeIterT begin, EdgeIterT end) {
    for (EdgeIterT ei = begin; ei != end; ++ei) {
      auto dn_next = *ei;
      llvm::Value *value = dn_next->getValue();
      llvm::Instruction *inst = dyn_cast<Instruction>(value);
      if (!inst || !shouldSliceInst(inst)) continue;
      auto sn_next = sg->getOrCreateNode(inst);
      sg->connect(sn_curr, sn_next, kind);
      enqueue(dn_next);
    }
  }

 protected:
  static void mark(dg::LLVMNode *n, uint32_t slice_id);
};

} // namespace slicing
} // namespace llvm

#endif /* _SLICING_DGWALK_H_ */
