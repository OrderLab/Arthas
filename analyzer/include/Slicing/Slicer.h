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

// We support two kinds of slicing approach:
//   1) storing: walk the dependency graph and storing the result into a 
//      SliceGraph data structure. The original dependency graph is intact.
//      This approach incurs some additional memory overhead, but is very
//      efficient to use and query the slices. It also preserves the ordering
//      of the dependencies among the slice elements.
//   2) marking: walk the dependency graph and mark elements (nodes, basic
//      blocks, functions) along the way. The result is embedded in the graph
//      and no additional SliceGraph is created. To retrieve the result, 
//      one has to walk the dependency graph again to check if an element
//      has been marked or not. This approach saves some memory overhead, but
//      it is inefficient to use and query the slices. It also loses the
//      ordering of dependencies, which can be useful.
enum class SlicingApproachKind { Storing, Marking };

enum SlicerDgFlags {
  ENTRY_ONLY = 1 << 0,
  INTRA_PROCEDURAL = 1 << 1,
  INTER_PROCEDURAL = 1 << 2,
  ENABLE_PTA = 1 << 3,
  ENABLE_CONTROL_DEP = 1 << 4,
  SUPPORT_THREADS = 1 << 5,
};

// Slicer based on dependency graph
class DgSlicer {
 public:
  typedef const std::map<llvm::Value *, dg::LLVMDependenceGraph *> FunctionDgMap;
  typedef void (*NodeSliceFunc) (dg::LLVMNode *);
  typedef void (*BasicBlockSliceFunc) (dg::BBlock<dg::LLVMNode> *);
  typedef void (*FunctionSliceFunc) (dg::LLVMDependenceGraph *);

 public:
  DgSlicer(llvm::Module *m, SliceDirection d)
      : _module(m), _direction(d), _last_slice_id(0), 
        _dependency_computed(false), _dg(nullptr), _funcDgMap(nullptr) {}

  bool computeDependencies(dg::llvmdg::LLVMDependenceGraphOptions &options);

  static dg::llvmdg::LLVMDependenceGraphOptions createDgOptions(
      uint32_t dg_flags, llvm::Function *entry = nullptr);

  dg::LLVMDependenceGraph *getDependenceGraph(llvm::Function *func);

  SliceGraph *slice(dg::LLVMNode *start, uint32_t &slice_id,
                    SlicingApproachKind kind,
                    uint32_t slice_dep_flags = DEFAULT_DEPENDENCY_FLAGS);

  SliceGraph *slice(llvm::Instruction *start, uint32_t &slice_id,
                    SlicingApproachKind kind,
                    uint32_t slice_dep_flags = DEFAULT_DEPENDENCY_FLAGS);

  SliceGraph *buildSliceGraph(dg::LLVMNode *start, uint32_t slice_id,
                              uint32_t slice_dep_flags);

  // functions for the marking approach -- only slide id is set on the
  // dependency graph node, no additional graph is constructed.
  uint32_t markSliceId(dg::LLVMNode *start, uint32_t slice_id,
                       uint32_t slice_dep_flags);
  void walkSliceId(uint32_t slice_id, dg::LLVMDependenceGraph *graph = nullptr,
                   NodeSliceFunc nodeFunc = nullptr,
                   BasicBlockSliceFunc bbFunc = nullptr,
                   FunctionSliceFunc fnFunc = nullptr);
  uint32_t lastSliceId() { return _last_slice_id; }

  dg::analysis::SlicerStatistics &getStatistics() { return _statistics; }
  const dg::analysis::SlicerStatistics &getStatistics() const
  {
    return _statistics;
  }
  SliceDirection getSliceDirection() const { return _direction; }

 protected:

  void walkNodeSliceId(dg::LLVMDependenceGraph *graph, uint32_t slice_id,
                       NodeSliceFunc func);
  void walkBasicBlockSliceId(dg::LLVMDependenceGraph *graph, uint32_t slice_id,
                             BasicBlockSliceFunc func);
  void walkFunctionSliceId(dg::LLVMDependenceGraph *graph, uint32_t slice_id,
                           FunctionSliceFunc func);
  void updateStatsSliceId(dg::LLVMDependenceGraph *graph, uint32_t slice_id);

 protected:
  llvm::Module *_module;
  SliceDirection _direction;
  uint32_t _last_slice_id;
  bool _dependency_computed;

  std::unique_ptr<dg::LLVMDependenceGraph> _dg;
  FunctionDgMap *_funcDgMap;
  // We need to hold a reference to the dg builder before the slicer is destroyed.
  // This is because the builder holds a unique_ptr to the LLVMPointerAnalysis.
  // If we need to use the PTA from the dg later, the PTA data structure memory
  // will become invalid and likely cause core dump when using it.
  std::unique_ptr<dg::llvmdg::LLVMDependenceGraphBuilder> _builder;
  dg::analysis::SlicerStatistics _statistics;
};

} // namespace slicing
} // namespace llvm

#endif /* _SLICING_SLICER_H_ */
