// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Slicing/Slice.h"
#include "Matcher/Matcher.h"

#include <algorithm>
#include <functional>
#include <set>

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

bool SliceValueComparator::operator()(
    const Slice::CompleteValueTy &val1,
    const Slice::CompleteValueTy &val2) const {
  auto dist1 = val1.second;
  auto dist2 = val2.second;
  // with a sequence of -4, -3, -2, -1, 0, 1, 2, 3, where 0 is the target
  // instruction, the backward sort order is:
  // 0, -1, -2, -3, -4, 1, 2, 3
  if (dist1 == 0 && dist2 == 0) return false;
  if (dist1 < 0) {
    if (dist2 > 0) return true;
    return -dist1 < -dist2;
  } else {
    if (dist2 < 0) return false;
    return dist1 < dist2;
  }
  return true;
}

void Slice::dump(raw_ostream &os)
{
  os << "Slice " << id << " (" << persistence << "):\n";
  for (auto i = begin(); i != end(); ++i) {
    Value *val = i->first;
    os << "~";
    if (auto inst = dyn_cast<Instruction>(val)) {
      os << inst->getFunction()->getName() << "()=>";
    }
    os << *val << "\n";
  }
}

Slice *Slice::fork() {
  Slice *copy = new Slice(id, root, direction, persistence, dependence);
  for (auto &val : *this) {
    ValueTy dep = val.first;
    // root has been inserted in the dep_vals, skip it
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
    Value *val = si->first;
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

void Slice::sort() {
  // if this is a backward slice, we should sort the slice based on reverse
  // program order. otherwise sort it based on program order
  //
  // FIXME: simply file:line sorting is insufficient
  std::sort(dep_vals.begin(), dep_vals.end(), SliceValueComparator());
}

Slices::~Slices() {
  // Assume Slices are the owner of all the slices. Therefore, it must release
  // the memory of each dynamically allocated slice in its destructor.
  for (slice_iterator si = begin(); si != end(); ++si) {
    Slice *slice = *si;
    delete slice;
  }
  slices.clear();
  sliceMap.clear();
}
