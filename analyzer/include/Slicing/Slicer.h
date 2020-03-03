// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _SLICING_SLICER_H_
#define _SLICING_SLICER_H_

#include "PMem/Extractor.h"
#include "Slicing/Slice.h"
#include "Slicing/SliceGraph.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CallSite.h"

#include "dg/llvm/LLVMDependenceGraph.h"
#include "dg/llvm/LLVMDependenceGraphBuilder.h"
#include "dg/llvm/LLVMNode.h"
#include "dg/llvm/LLVMSlicer.h"
#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"
//#include "dg/llvm/LLVMDG2Dot.h"
#include "dg/analysis/Slicing.h"
#include "dg/analysis/legacy/Analysis.h"
#include "dg/analysis/legacy/NodesWalk.h"
#include "dg/analysis/legacy/BFS.h"
#include "dg/ADT/Queue.h"
#include "dg/DependenceGraph.h"
//#include "dg/DG2Dot.h"
//#include "dg/analysis/legacy/SliceGraph.h"

#include "dg/util/TimeMeasure.h"

#include "ws/Languages/LLVM.h"

namespace llvm {
namespace slicing {


// this class will go through the nodes
// and will mark the ones that should be in the slice
class DgWalkAndMark
  : public dg::analysis::legacy::NodesWalk<dg::LLVMNode, 
    dg::ADT::QueueFIFO<dg::LLVMNode *>> {

  using Queue = dg::ADT::QueueFIFO<dg::LLVMNode *>;
  using DgBasicBlock = dg::BBlock<dg::LLVMNode>;

 public:
  DgWalkAndMark(SliceDirection dir)
      : dg::analysis::legacy::NodesWalk<dg::LLVMNode, Queue>(
            sliceRelationOpts(dir)),
        _dir(dir) {}

  void mark(const std::set<dg::LLVMNode *> &start, uint32_t slice_id,
            llvm::slicing::SliceGraph *sg);

  inline bool isForward() const { return _dir == SliceDirection::Forward; }
  inline bool isBackward() const { return _dir == SliceDirection::Backward; }
  inline bool isFull() const { return _dir == SliceDirection::Full; }

  static uint32_t sliceRelationOpts(SliceDirection dir);

  const std::set<DgBasicBlock *>& getMarkedBlocks() { return _markedBlocks; }

 private:
  std::set<DgBasicBlock *> _markedBlocks;
  SliceDirection _dir;

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

// Slicer based on dependency graph
class DgSlicer : public dg::analysis::Slicer<dg::LLVMNode>
{
  typedef const std::map<llvm::Value *, dg::LLVMDependenceGraph *> FunctionDgMap;

 public:
  DgSlicer(llvm::Module *m, SliceDirection d = SliceDirection::Full)
      : _module(m), _direction(d), _dg(nullptr), _funcDgMap(nullptr), 
      _dependency_computed(false) {}

  std::set<DgSlice *> slices;

  bool computeDependencies();
  dg::LLVMDependenceGraph *getDependenceGraph(llvm::Function *func);
  uint32_t slice(dg::LLVMNode *start, SliceGraph *sg, uint32_t sl_id = 0);
  void sliceGraph(dg::LLVMDependenceGraph *graph, uint32_t slice_id);

  bool removeBlock(dg::LLVMBBlock *block) override;
  bool removeNode(dg::LLVMNode *node) override;

 protected:
 private:
  llvm::Module *_module;
  SliceDirection _direction;
  std::unique_ptr<dg::LLVMDependenceGraph> _dg;
  FunctionDgMap *_funcDgMap;
  SlicePersistence _persistent_state;
  bool _dependency_computed;

  uint64_t _slice_id;

  // We need to hold a reference to the dg builder before the slicer is destroyed.
  // This is because the builder holds a unique_ptr to the LLVMPointerAnalysis.
  // If we need to use the PTA from the dg later, the PTA data structure memory
  // will become invalid and likely cause core dump when using it.
  std::unique_ptr<dg::llvmdg::LLVMDependenceGraphBuilder> _builder;
};



} // namespace slicing
} // namespace llvm

#endif /* _SLICING_SLICER_H_ */
