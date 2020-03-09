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


raw_ostream &operator<<(raw_ostream &os, const SlicePersistence & persistence)
{
  switch(persistence) {
    case SlicePersistence::NA: os << "n/a"; return os;
    case SlicePersistence::Persistent: os << "persistent"; return os;
    case SlicePersistence::Volatile: os << "volatile"; return os;
    case SlicePersistence::Mixed: os << "mixed"; return os;
    default: os << "unknown"; return os;
  }
}

raw_ostream &operator<<(raw_ostream &os, const SliceDirection & direction)
{
  switch(direction) {
    case SliceDirection::Backward: os << "backward"; return os;
    case SliceDirection::Forward: os << "forward"; return os;
    case SliceDirection::Full: os << "backward+forward"; return os;
    default: os << "unknown"; return os;
  }
}

void Slice::dump(raw_ostream &os)
{
  os << "Slice " << id << ":\n";
  for (auto i = begin(); i != end(); ++i) {
    Value *val = *i;
    os << *val << "\n";
  }
  os << "Slice " << id << " is " << persistence << "\n";
}

Slice *Slice::fork() {
  Slice *copy = new Slice(id, root, direction, persistence);
  for (Value *dep : *this) {
    // root has been inserted in the dep_values, skip it
    if (dep == root)
      continue;
    copy->add(dep);
  }
  return copy;
}

void Slice::setPersistence(llvm::ArrayRef<llvm::Value *> persist_vars) {
  bool vol = false;
  bool persistent = false;
  for (auto si = begin(); si != end(); ++si) {
    Value *val = *si;
    for (auto pi = persist_vars.begin(); pi != persist_vars.end(); ++pi) {
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
