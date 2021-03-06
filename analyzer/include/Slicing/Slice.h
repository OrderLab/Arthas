// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _SLICING_SLICE_H_
#define _SLICING_SLICE_H_

#include <map>
#include <vector>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
namespace slicing {

enum class SlicePersistence { NA, Persistent, Volatile, Mixed };

enum class SliceDirection { Backward, Forward, Full };

enum class SliceDependence {
  Unknown,
  RegisterDefUse,
  MemoryDependence,
  ControlDependence,
  InterfereDependence,
};

enum SliceDependenceFlags {
  DEF_USE = 1 << 0,
  MEMORY = 1 << 1,
  CONTROL = 1 << 2,
  INTERFERENCE = 1 << 3,
};

#define DEFAULT_DEPENDENCY_FLAGS \
  SliceDependenceFlags::DEF_USE | SliceDependenceFlags::MEMORY

#define FULL_DEPENDENCY_FLAGS                                    \
  SliceDependenceFlags::DEF_USE | SliceDependenceFlags::MEMORY | \
      SliceDependenceFlags::CONTROL | SliceDependenceFlags::INTERFERENCE

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const SlicePersistence &persistence) {
  switch (persistence) {
    case SlicePersistence::NA:
      os << "n/a";
      return os;
    case SlicePersistence::Persistent:
      os << "persistent";
      return os;
    case SlicePersistence::Volatile:
      os << "volatile";
      return os;
    case SlicePersistence::Mixed:
      os << "mixed";
      return os;
    default:
      os << "unknown";
      return os;
  }
}

class Slice {
 public:
  using ValueTy = llvm::Instruction *;
  typedef int64_t DistanceTy;
  using CompleteValueTy = std::pair<ValueTy, DistanceTy>;

  typedef llvm::SmallVector<CompleteValueTy, 20> DependentValueList;

  typedef DependentValueList::iterator dep_iterator;
  typedef DependentValueList::const_iterator dep_const_iterator;

  uint64_t id;
  ValueTy root;
  SliceDirection direction;
  SlicePersistence persistence;
  SliceDependence dependence;

  DependentValueList dep_vals;

  Slice(uint64_t slice_id, ValueTy root_val, SliceDirection dir,
        SlicePersistence kind = SlicePersistence::NA,
        SliceDependence dep = SliceDependence::Unknown)
      : id(slice_id), root(root_val), direction(dir), persistence(kind),
        dependence(dep) {
    dep_vals.push_back(
        std::make_pair(root_val, 0));  // root val depends on itself
  }

  Slice *fork();

  inline void add(ValueTy inst) { dep_vals.push_back(std::make_pair(inst, 0)); }

  inline dep_iterator begin() { return dep_vals.begin(); }
  inline dep_iterator end() { return dep_vals.end(); }
  inline dep_const_iterator begin() const { return dep_vals.begin(); }
  inline dep_const_iterator end() const { return dep_vals.end(); }

  void sort();
  void print_slice_persistence();

  void setPersistence(llvm::SetVector<llvm::Value *> persist_vars);
  void dump(llvm::raw_ostream &os);
};

struct SliceValueComparator {
  bool operator()(const Slice::CompleteValueTy &val1,
                  const Slice::CompleteValueTy &val2) const;
};

class Slices {
 public:
  typedef std::vector<Slice *> SliceList;
  typedef std::map<uint64_t, Slice *> SliceMap;
  typedef SliceList::iterator slice_iterator;
  typedef SliceList::const_iterator slice_const_iterator;

  Slices() {}
  ~Slices();

  inline slice_iterator begin() { return slices.begin(); }
  inline slice_iterator end() { return slices.end(); }
  inline slice_const_iterator begin() const { return slices.begin(); }
  inline slice_const_iterator end() const { return slices.end(); }
  inline SliceList &vec() { return slices; }

  inline size_t size() const { return slices.size(); }
  inline bool empty() const { return slices.empty(); }

  Slice *get(uint64_t slice_id) {
    auto si = sliceMap.find(slice_id);
    if (si == sliceMap.end()) return nullptr;
    return si->second;
  }

  inline void add(Slice *slice) {
    sliceMap.insert(std::make_pair(slice->id, slice));
    slices.push_back(slice);
  }

  bool has(uint64_t slice_id) {
    return sliceMap.find(slice_id) != sliceMap.end();
  }

 protected:
  SliceList slices;
  SliceMap sliceMap;
};

}  // namespace slicing
}  // namespace llvm

llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os, const llvm::slicing::SlicePersistence &persistence);

llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                              const llvm::slicing::SliceDirection &direction);

#endif /* _SLICING_SLICE_H_ */
