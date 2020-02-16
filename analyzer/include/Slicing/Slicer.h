// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef __SLICER_H_
#define __SLICER_H_

#include "PMem/Extractor.h"
#include "Slicing/Slice.h"

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

// Slicer based on dependency graph
class DgSlicer {
  typedef const std::map<llvm::Value *, dg::LLVMDependenceGraph *> FunctionDgMap;

 public:
  DgSlicer(llvm::Module *m, SliceDirection d=SliceDirection::Full): 
    module(m), direction(d), dg(nullptr), funcDgMap(nullptr) {}
  std::set<DgSlice *> slices;

  bool compute();

  dg::LLVMDependenceGraph *getDependenceGraph(llvm::Function *func);

 private:
  llvm::Module *module;
  SliceDirection direction;
  std::unique_ptr<dg::LLVMDependenceGraph> dg;
  FunctionDgMap *funcDgMap;
  SlicePersistence persistent_state;
  uint64_t slice_id;
  // We need to hold a reference to the dg builder before the slicer is destroyed.
  // This is because the builder holds a unique_ptr to the LLVMPointerAnalysis.
  // If we need to use the PTA from the dg later, the PTA data structure memory
  // will become invalid and likely cause core dump when using it.
  std::unique_ptr<dg::llvmdg::LLVMDependenceGraphBuilder> builder;
};



} // namespace slicing
} // namespace llvm

#endif /* __SLICER_H_ */
