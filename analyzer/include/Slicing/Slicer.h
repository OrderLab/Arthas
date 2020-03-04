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

#include "dg/ADT/Queue.h"
#include "dg/analysis/Slicing.h"
#include "dg/util/TimeMeasure.h"

namespace llvm {
namespace slicing {

// Slicer based on dependency graph
class DgSlicer : public dg::analysis::Slicer<dg::LLVMNode> {
  typedef const std::map<llvm::Value *, dg::LLVMDependenceGraph *> FunctionDgMap;

 public:
  DgSlicer(llvm::Module *m, SliceDirection d)
      : _module(m), _direction(d), _dg(nullptr), _funcDgMap(nullptr), 
      _dependency_computed(false), _last_slice_id(0) {}

  bool computeDependencies();
  dg::LLVMDependenceGraph *getDependenceGraph(llvm::Function *func);

  uint32_t markSliceId(dg::LLVMNode *start, uint32_t slice_id = 0);
  uint32_t slice(dg::LLVMNode *start, SliceGraph *sg, uint32_t slice_id = 0);
  void sliceGraph(dg::LLVMDependenceGraph *graph, uint32_t slice_id);
  inline uint32_t lastSliceId() { return _last_slice_id; }

  bool removeBlock(dg::LLVMBBlock *block) override;
  bool removeNode(dg::LLVMNode *node) override;

 private:
  llvm::Module *_module;
  SliceDirection _direction;
  std::unique_ptr<dg::LLVMDependenceGraph> _dg;
  FunctionDgMap *_funcDgMap;
  SlicePersistence _persistent_state;
  bool _dependency_computed;
  uint32_t _last_slice_id;

  // We need to hold a reference to the dg builder before the slicer is destroyed.
  // This is because the builder holds a unique_ptr to the LLVMPointerAnalysis.
  // If we need to use the PTA from the dg later, the PTA data structure memory
  // will become invalid and likely cause core dump when using it.
  std::unique_ptr<dg::llvmdg::LLVMDependenceGraphBuilder> _builder;
};

} // namespace slicing
} // namespace llvm

#endif /* _SLICING_SLICER_H_ */
