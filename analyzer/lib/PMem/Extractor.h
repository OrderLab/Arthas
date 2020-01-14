// The PMEM-Fault Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef __EXTRACTOR_H_
#define __EXTRACTOR_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"

namespace llvm {
namespace pmem {

class PMemVariableLocator {
  // Use the small vector here, reference on picking what ADT to use:
  // http://llvm.org/docs/ProgrammersManual.html#picking-the-right-data-structure-for-a-task
 public:
  typedef llvm::SmallVector<const llvm::CallInst *, 5> ApiCallList;
  typedef llvm::SmallVector<const llvm::Value *, 20> VariableList;
  typedef std::multimap<const llvm::Value *, const llvm::Value *> RegionList;
  typedef std::pair<const llvm::Value *, const llvm::Value *> RegionInfo;

 public:
  PMemVariableLocator(Function &F);

  ApiCallList::const_iterator call_begin() const { return callList.begin(); }
  ApiCallList::const_iterator call_end() const { return callList.end(); }

  ApiCallList::iterator call_begin() { return callList.begin(); }
  ApiCallList::iterator call_end() { return callList.end(); }

  VariableList::iterator var_begin() { return varList.begin(); }
  VariableList::iterator var_end() { return varList.end(); }

  VariableList::const_iterator var_begin() const { return varList.begin(); }
  VariableList::const_iterator var_end() const { return varList.end(); }

  RegionList::iterator region_begin() { return regionList.begin(); }
  RegionList::iterator region_end() { return regionList.end(); }

  RegionList::const_iterator region_begin() const { return regionList.begin(); }
  RegionList::const_iterator region_end() const { return regionList.end(); }

 private:
  ApiCallList callList;
  VariableList varList;
  RegionList regionList;

  static const std::set<std::string> pmdkApiSet;
  static const std::set<std::string> pmdkPMEMVariableReturnSet;
  static const std::set<std::string> memkindApiSet;
  static const std::set<std::string> memkindVariableReturnSet;
  static const std::map<std::string, unsigned int> pmdkRegionSizeArgMapping;
};

} // namespace pmem
} // namespace llvm

#endif /* __EXTRACTOR_H_ */
