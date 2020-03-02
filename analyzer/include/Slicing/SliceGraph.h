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
#include "llvm/Support/raw_ostream.h"

#include "dg/llvm/LLVMNode.h"

namespace llvm {
namespace slicing {

class SliceNode {
 public:
  typedef std::vector<SliceNode *> Children;
  typedef Children::iterator child_iterator;
  typedef Children::const_iterator child_const_iterator;

 public:
  dg::LLVMNode *node;
  int depth;
  Children child_nodes;
  dg::LLVMNode *prev_node;

  SliceNode(dg::LLVMNode *node, int node_depth) {
    this->node = node;
    depth = node_depth;
  }

  ~SliceNode() {
    for (child_iterator ci = child_begin(); ci != child_end(); ++ci) {
      // the child nodes are dynamically allocated, deallocate them when
      // a slice node is destroyed.
      SliceNode *sn = *ci;
      delete sn;
    }
  }

  inline child_iterator child_begin() { return child_nodes.begin(); }
  inline child_iterator child_end() { return child_nodes.end(); }
  inline child_const_iterator child_begin() const {
    return child_nodes.begin();
  }
  inline child_const_iterator child_end() const { return child_nodes.end(); }

  void add_child(dg::LLVMNode *n, int depth, dg::LLVMNode *prev_node) {
    // FIXME: memory leak
    SliceNode *sn = new SliceNode(n, depth);
    // llvm::errs() << "added value of " << n << "\n";
    sn->prev_node = prev_node;
    child_nodes.push_back(sn);
    // llvm::errs() << "added value of " << sn << "\n";
  }

  int total_size(SliceNode *sn) {
    int num = 1;
    for (auto i = sn->child_nodes.begin(); i != sn->child_nodes.end(); ++i) {
      num += total_size(*i);
    }
    return num;
  }

  SliceNode *search_children(dg::LLVMNode *n);
  void dump(raw_ostream &os);
  void dump(raw_ostream &os, int level);
  void slice_node_copy(DgSlice &base, DgSlices &slices);
  int compute_slices(DgSlices &slices, llvm::Instruction *fi, SliceDirection sd,
                     SlicePersistence sp, uint64_t slice_id);
};

class SliceGraph {
 public:
   typedef std::vector<SliceNode *> SliceNodeList;
   typedef SliceNodeList::iterator node_iterator;
   typedef SliceNodeList::const_iterator node_const_iterator;

 public:
   SliceGraph() {}
   ~SliceGraph();

   SliceNode *root;
   int maxDepth;

   inline node_iterator node_begin() { return nodeList.begin(); }
   inline node_iterator node_end() { return nodeList.end(); }
   inline node_const_iterator node_begin() const { return nodeList.begin(); }
   inline node_const_iterator node_end() const { return nodeList.end(); }

   bool compute_slices() { return false; }

   SliceNode *find_node(dg::LLVMNode *n) { return nullptr; }

   bool add_node() { return false; }

 protected:
   SliceNodeList nodeList;
};

} // namespace slicing
} // namespace llvm

#endif
