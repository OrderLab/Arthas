// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Slicing/Slice.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;

void DgSlice::dump(raw_ostream &os)
{
  os << "Slice " << slice_id << ":\n";
  for (auto i = dep_nodes.begin(); i != dep_nodes.end(); ++i) {
    dg::LLVMNode *n = *i;
    llvm::Value *v = n->getValue();
    os << *v << "\n";
  }
  os << "Slice " << slice_id << " is " << persistence << "\n";
}

void DgSlice::set_persistence(SmallVectorImpl<Value *> &persist_insts) {
  bool vol = false;
  bool persistent = false;
  for (auto di = dep_nodes.begin(); di != dep_nodes.end(); ++di) {
    dg::LLVMNode *n = *di;
    llvm::Value *v = n->getValue();
    llvm::Instruction *inst = dyn_cast<llvm::Instruction>(v);
    dep_instrs.push_back(inst);
    for (auto pi = persist_insts.begin(); pi != persist_insts.end(); ++pi) {
      if (v == *pi) {
        persistent = true;
      } else {
        vol = true;
      }
    }
  }
  if (vol && persistent) {
    persistence = SlicePersistence::Mixed;
  } else if (vol) {
    persistence = SlicePersistence::Volatile;
  } else if (persistent) {
    persistence = SlicePersistence::Persistent;
  }
}

DgSlices::~DgSlices()
{
  // Assume DgSlices are the owner of all the slices. Therefore, it must release
  // the memory of each dynamically allocated slice in its destructor.
  for (slice_iterator si = begin(); si != end(); ++si) {
    DgSlice *slice = *si;
    delete slice;
  }
}
