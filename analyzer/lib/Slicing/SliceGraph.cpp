// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Slicing/SliceGraph.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;

void SliceNode::dump()
{
  errs() << n << "\n";
  for (child_iterator i = child_nodes.begin(); i != child_nodes.end(); ++i) {
    SliceNode *sn = *i;
    sn->dump();
  }
}

void SliceNode::dump(int level)
{
  if (level == 0) {
    errs() << "root " << this->n << "\n";
  }

  for (child_iterator i = child_nodes.begin(); i != child_nodes.end(); ++i) {
    SliceNode *sn = *i;
    errs() << sn->n << " depth of " << level << "prev is " << this->n
      << "prev double check is " << sn->prev_node << "\n";
  }
  int new_level = level + 1;
  for (child_iterator i = child_nodes.begin(); i != child_nodes.end(); ++i) {
    SliceNode *sn = *i;
    sn->dump(new_level);
  }
}

SliceNode *SliceNode::search_children(dg::LLVMNode *n) 
{
  if (this->n == n) return this;
  for (auto i = child_nodes.begin(); i != child_nodes.end(); ++i) {
    SliceNode *sn = *i;
    if (sn->n == n) return sn;
    SliceNode *out = sn->search_children(n);
    if (out != nullptr) return out;
  }
  return nullptr;
}

void SliceNode::slice_node_copy(DgSlice &base, DgSlices &slices)
{
  DgSlice *s;
  for (auto i = slices.begin(); i != slices.end(); ++i) {
    s = &*i;
    // if(this->prev_node == s->highest_node || slices.size() == 1){
    if (n == s->root_node || slices.size() == 1) {
      // errs() << "prev node is " << this->prev_node << "current node
      // is " << this->n << "\n";
      // errs() << "s highest nodes is " << s->highest_node << "\n";
      // errs() << "before copy "<< s->nodes.size() << "\n";
      base.dep_nodes = s->dep_nodes;
      // errs() << "after copy "<< s->nodes.size() << "\n";
      // errs() << "Found slice\n";
      break;
    }
  }
}

int SliceNode::compute_slices(DgSlices &slices, llvm::Instruction *fi,
                              SliceDirection sd, SlicePersistence sp,
                              uint64_t slice_id) {
  uint64_t slice_num = slice_id;
  // Base Case: Adding in a slice of just the root node of the graph
  if (slices.empty()) {
    DgSlice dgs = DgSlice(fi, sd, sp, slice_id, n);
    dgs.dep_nodes.push_back(n);
    dgs.root_node = n;
    slices.push_back(dgs);
    slice_num++;
  }

  // Creating base slice to copy, need to change slice_id and highest_node
  // DGSlice base = DGSlice(fi, sd, sp, slice_num, this->n);
  DgSlice *s;
  for (auto i = slices.begin(); i != slices.end(); ++i) {
    s = &*i;
    // if (this->prev_node == s->highest_node || slices.size() == 1){
    if (this->n == s->root_node || slices.size() == 1) {
      // errs() << "prev node is " << this->prev_node << "current node
      // is " << this->n << "\n";
      // errs() << "s highest nodes is " << s->highest_node << "\n";
      // errs() << "before copy "<< s->nodes.size()  << "\n";
      // base.nodes = s->nodes;
      // errs() << "after copy "<< s->nodes.size()  << "\n";
      // errs() << "Found slice\n";
      break;
    }
  }

  /*if (base.nodes.size() == 0)
    errs() << "problem: prev node is " << this->prev_node << "current
    node is " << this->n << "\n";
    */

  int children_num = this->child_nodes.size();
  int child_count = 0;
  // Iterating through all of the children to create more slices
  for (child_iterator i = this->child_nodes.begin();
      i != this->child_nodes.end(); ++i) {
    SliceNode *sn = *i;
    child_count++;
    // errs() << "child of " << this->n << " is " << sn->n << "\n";
    // errs() << "child_count is " << child_count << " children count is ";
    // errs() << children_num << "\n";
    if (child_count == children_num) {
      // errs() << "pushing to existing branch\n";
      s->dep_nodes.push_back(sn->n);
      s->root_node = sn->n;
      // errs() << "number of slices is " << slices.size() << "\n";
    } else {
      DgSlice base = DgSlice(fi, sd, sp, slice_num, this->n);
      this->slice_node_copy(base, slices);
      // errs() << "new slice old highest is " << base.highest_node << "\n";
      // errs() << "base nodes size is " << base.nodes.size() << "\n";
      base.slice_id = slice_num;
      base.root_node = sn->n;
      base.dep_nodes.push_back(sn->n);
      slice_num++;
      slices.push_back(base);
      // errs() << "number of slices is " << slices.size() << "\n";
    }
    slice_num = sn->compute_slices(slices, fi, sd, sp, slice_num);
  }
  return slice_num;
}
