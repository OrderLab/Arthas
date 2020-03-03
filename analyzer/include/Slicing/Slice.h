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
#include "llvm/Support/raw_ostream.h"

#include "dg/llvm/LLVMDependenceGraph.h"
#include "dg/llvm/LLVMNode.h"

namespace llvm {
namespace slicing {

enum class SlicePersistence { NA, Persistent, Volatile, Mixed };

enum class SliceDirection { Backward, Forward, Full };

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, 
    const SlicePersistence & persistence)
{
  switch(persistence) {
    case SlicePersistence::NA: os << "n/a"; return os;
    case SlicePersistence::Persistent: os << "persistent"; return os;
    case SlicePersistence::Volatile: os << "volatile"; return os;
    case SlicePersistence::Mixed: os << "mixed"; return os;
    default: os << "unknown"; return os;
  }
}

class DgSlice {
 public:
  typedef llvm::SmallVector<llvm::Value *, 20> DependentValueList;

  typedef DependentValueList::iterator dep_iterator;
  typedef DependentValueList::const_iterator dep_const_iterator;

  uint64_t id;
  llvm::Value *root;
  SliceDirection direction;
  SlicePersistence persistence;
  DependentValueList dep_values;

  DgSlice(uint64_t slice_id, llvm::Value *root_val, SliceDirection dir,
          SlicePersistence kind)
      : id(slice_id), root(root_val), direction(dir)
  {
    dep_values.push_back(root_val);  // root val depends on itself
  }

  inline void add(llvm::Value *val) { dep_values.push_back(val); }

  inline dep_iterator begin() { return dep_values.begin(); }
  inline dep_iterator end() { return dep_values.end(); }
  inline dep_const_iterator begin() const { return dep_values.begin(); }
  inline dep_const_iterator end() const { return dep_values.end(); }

  void setPersistence(llvm::SmallVectorImpl<llvm::Value *> &persist_vals);
  void dump(llvm::raw_ostream &os);
};

class DgSlices {
 public:
  typedef std::vector<DgSlice *> SliceList;
  typedef std::map<uint64_t, DgSlice *> SliceMap;
  typedef SliceList::iterator slice_iterator;
  typedef SliceList::const_iterator slice_const_iterator;

  DgSlices() {}
  ~DgSlices();

  inline slice_iterator begin() { return slices.begin(); }
  inline slice_iterator end() { return slices.end(); }
  inline slice_const_iterator begin() const { return slices.begin(); }
  inline slice_const_iterator end() const { return slices.end(); }
  inline SliceList &vec() { return slices; }

  inline size_t size() const { return slices.size(); }
  inline bool empty() const { return slices.empty(); }

  inline void add(DgSlice *slice)
  { 
    sliceMap.insert(std::make_pair(slice->id, slice));
    slices.push_back(slice);
  }

 protected:
  SliceList slices;
  SliceMap sliceMap;
};

} // namespace slicing
} // namespace llvm

#endif /* _SLICING_SLICE_H_ */
