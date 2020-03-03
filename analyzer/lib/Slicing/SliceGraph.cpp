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

void SliceNode::dump(raw_ostream &os)
{
  os << node << "\n";
  for (child_iterator i = child_nodes.begin(); i != child_nodes.end(); ++i) {
    SliceNode *sn = *i;
    sn->dump(os);
  }
}

void SliceNode::dump(raw_ostream &os, int level)
{
  if (level == 0) {
    os << "root " << node << "\n";
  }

  for (child_iterator i = child_nodes.begin(); i != child_nodes.end(); ++i) {
    SliceNode *sn = *i;
    os << sn->node << " depth of " << level << ", prev is " << node << "\n";
    if (node != sn->prev_node) {
      errs() << "Warning: inconsistent prev node " << sn->prev_node << "\n";
    }
  }
  int new_level = level + 1;
  for (child_iterator i = child_nodes.begin(); i != child_nodes.end(); ++i) {
    SliceNode *sn = *i;
    sn->dump(os, new_level);
  }
}

SliceNode *SliceNode::search_children(dg::LLVMNode *n) 
{
  if (node == n) return this;
  for (auto i = child_nodes.begin(); i != child_nodes.end(); ++i) {
    SliceNode *sn = *i;
    if (sn->node == n) return sn;
    SliceNode *out = sn->search_children(n);
    if (out != nullptr) return out;
  }
  return nullptr;
}

void SliceNode::slice_node_copy(DgSlice &base, DgSlices &slices)
{
  DgSlice *s;
  for (auto i = slices.begin(); i != slices.end(); ++i) {
    s = *i;
    // if(this->prev_node == s->highest_node || slices.size() == 1){
    if (node->getValue() == s->root || slices.size() == 1) {
      // errs() << "prev node is " << this->prev_node << "current node
      // is " << this->n << "\n";
      // errs() << "s highest nodes is " << s->highest_node << "\n";
      // errs() << "before copy "<< s->nodes.size() << "\n";
      base.dep_values = s->dep_values;
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
    DgSlice *slice = new DgSlice(slice_id, fi, sd, sp);
    slices.add(slice);
    slice_num++;
  }

  // Creating base slice to copy, need to change slice_id and highest_node
  // DGSlice base = DGSlice(fi, sd, sp, slice_num, this->n);
  DgSlice *s;
  for (auto i = slices.begin(); i != slices.end(); ++i) {
    s = *i;
    // if (this->prev_node == s->highest_node || slices.size() == 1){
    if (node->getValue() == s->root || slices.size() == 1) {
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
    node is " << node << "\n";
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
      s->add(sn->node->getValue());
      //TODO: what's this for?
      s->root = sn->node->getValue();
      // errs() << "number of slices is " << slices.size() << "\n";
    } else {
      DgSlice *base = new DgSlice(slice_num, fi, sd, sp);
      this->slice_node_copy(*base, slices);
      // errs() << "new slice old highest is " << base.highest_node << "\n";
      // errs() << "base nodes size is " << base.nodes.size() << "\n";
      slice_num++;
      slices.add(base);
      // errs() << "number of slices is " << slices.size() << "\n";
    }
    slice_num = sn->compute_slices(slices, fi, sd, sp, slice_num);
  }
  return slice_num;
}

SliceGraph::~SliceGraph()
{ 
  delete root;
}
