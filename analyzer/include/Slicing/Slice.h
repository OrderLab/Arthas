// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _SLICING_SLICE_H_
#define _SLICING_SLICE_H_

#include <vector>

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"

#include "dg/llvm/LLVMDependenceGraph.h"
#include "dg/llvm/LLVMNode.h"

namespace llvm {
namespace slicing {

enum class SlicePersistence { Persistent, Volatile, Mixed };

enum class SliceDirection { Backward, Forward, Full };

class DgSlice {
 public:
  typedef llvm::SmallVector<const llvm::Instruction *, 20> DependentInstrs;
  typedef llvm::SmallVector<dg::LLVMNode *, 20> DependentNodes;

  SliceDirection direction;
  SlicePersistence persistence;
  uint64_t slice_id;

  DependentInstrs dep_instrs;
  llvm::Instruction *root_instr;
  DependentNodes dep_nodes;
  dg::LLVMNode *root_node;

  DgSlice() {}

  DgSlice(llvm::Instruction *root, SliceDirection direction,
          SlicePersistence persistence, uint64_t slice_id,
          dg::LLVMNode *node) {
    root_instr = root;
    direction = direction;
    persistence = persistence;
    slice_id = slice_id;
    root_node = node;
  }

  void dump();
  void set_persistence(std::vector<llvm::Instruction *> &persist_instrs);
};

typedef std::vector<DgSlice> DgSlices;

} // namespace slicing
} // namespace llvm

#endif /* _SLICING_SLICE_H_ */
