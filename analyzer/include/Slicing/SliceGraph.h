// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _SLICING_SLICEGRAPH_H_
#define _SLICING_SLICEGRAPH_H_

#include <queue>
#include <stack>
#include <utility>
#include <vector>

#include "Slicing/Slice.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
namespace slicing {

class SliceNode {
 public:
  typedef std::vector<SliceNode *> Children;
  typedef Children::iterator child_iterator;
  typedef Children::const_iterator child_const_iterator;

 public:
  llvm::Value *value;
  int depth;
  Children child_nodes;

  SliceNode(llvm::Value *val, int node_depth) {
    value = val;
    depth = node_depth;
  }

  inline child_iterator child_begin() { return child_nodes.begin(); }
  inline child_iterator child_end() { return child_nodes.end(); }
  inline child_const_iterator child_begin() const {
    return child_nodes.begin();
  }
  inline child_const_iterator child_end() const { return child_nodes.end(); }

  void add_child(SliceNode *child) {
    child_nodes.push_back(child);
  }

  int total_size(SliceNode *sn) {
    int num = 1;
    for (auto i = sn->child_nodes.begin(); i != sn->child_nodes.end(); ++i) {
      num += total_size(*i);
    }
    return num;
  }

  SliceNode *search_children(llvm::Value *val);
  void dump(raw_ostream &os);
  void dump(raw_ostream &os, int level);
  void slice_node_copy(Slice &base, Slices &slices);
  int compute_slices(Slices &slices, llvm::Instruction *fi, SliceDirection sd,
                     SlicePersistence sp, uint64_t slice_id);
};

class SliceGraph {
 public:
   typedef std::vector<SliceNode *> SliceNodeList;
   typedef SliceNodeList::iterator node_iterator;
   typedef SliceNodeList::const_iterator node_const_iterator;

 public:
  SliceGraph(SliceNode *root_node) : root(root_node) {}

  ~SliceGraph();

  SliceNode *root;
  int maxDepth;

  inline node_iterator node_begin() { return nodeList.begin(); }
  inline node_iterator node_end() { return nodeList.end(); }
  inline node_const_iterator node_begin() const { return nodeList.begin(); }
  inline node_const_iterator node_end() const { return nodeList.end(); }

 protected:
  SliceNodeList nodeList;
};

} // namespace slicing
} // namespace llvm

#endif
