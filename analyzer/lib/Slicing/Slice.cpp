// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "llvm/Support/raw_ostream.h"

#include "Slicing/Slice.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;

void DgSlice::dump() 
{
  // errs() << "Slice " << slice_id << " \n";
  for(auto i = dep_nodes.begin(); i != dep_nodes.end(); ++i){
    // errs() << *i << "\n";
    dg::LLVMNode *n = *i;
    llvm::Value *v = n->getValue();
    errs() << *v << "\n";
  }
  // errs() << "slice persistence is " <<
  // static_cast<int>(persistent_state) << "\n";
}

void DgSlice::set_persistence(vector<Instruction *> &persist_instrs)
{
  bool vol = false;
  bool persistent = false;
  for (auto di = dep_nodes.begin(); di != dep_nodes.end(); ++di) {
    dg::LLVMNode *n = *di;
    llvm::Value *v = n->getValue();
    llvm::Instruction *inst = dyn_cast<llvm::Instruction>(v);
    dep_instrs.push_back(inst);
    for (auto pi = persist_instrs.begin(); pi != persist_instrs.end(); ++pi) {
      if (inst == *pi) {
        persistent = true;
      } else {
        vol = true;
      }
    }
  }
  if (vol && persistent) {
    errs() << "Slice " << slice_id << " is mixed\n";
    persistence = SlicePersistence::Mixed;
  } else if (vol) {
    errs() << "Slice " << slice_id << " is volatile\n";
    persistence = SlicePersistence::Volatile;
  } else if (persistent) {
    errs() << "Slice " << slice_id << " is persistent\n";
    persistence = SlicePersistence::Persistent;
  }
}
