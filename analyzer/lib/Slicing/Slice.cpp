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

void Slice::dump(raw_ostream &os)
{
  os << "Slice " << id << ":\n";
  for (auto i = begin(); i != end(); ++i) {
    Value *val = *i;
    os << *val << "\n";
  }
  os << "Slice " << id << " is " << persistence << "\n";
}

void Slice::setPersistence(SmallVectorImpl<Value *> &persist_vals) {
  bool vol = false;
  bool persistent = false;
  for (auto si = begin(); si != end(); ++si) {
    Value *val = *si;
    for (auto pi = persist_vals.begin(); pi != persist_vals.end(); ++pi) {
      if (val == *pi) {
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

Slices::~Slices()
{
  // Assume Slices are the owner of all the slices. Therefore, it must release
  // the memory of each dynamically allocated slice in its destructor.
  for (slice_iterator si = begin(); si != end(); ++si) {
    Slice *slice = *si;
    delete slice;
  }
  slices.clear();
  sliceMap.clear();
}
