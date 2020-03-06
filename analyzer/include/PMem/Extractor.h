// The Arthas Project
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
  typedef llvm::SmallVectorImpl<llvm::CallInst *> ApiCallListImpl;
  typedef llvm::SmallVectorImpl<llvm::Value *> VariableListImpl;
  typedef llvm::SmallVector<llvm::CallInst *, 5> ApiCallList;
  typedef llvm::SmallVector<llvm::Value *, 20> VariableList;

  typedef std::multimap<llvm::Value *, llvm::Value *> RegionList;
  typedef std::pair<llvm::Value *, llvm::Value *> RegionInfo;
  typedef std::map<llvm::Instruction *, std::set<llvm::Value *>> UseDefMap;
  typedef std::set<llvm::Value *> DefinitionPoints;
  typedef std::set<llvm::Value *> ProcessedUses;

  typedef ApiCallListImpl::iterator api_iterator;
  typedef ApiCallListImpl::const_iterator api_const_iterator;
  typedef VariableListImpl::iterator var_iterator;
  typedef VariableListImpl::const_iterator const_var_iterator;
  typedef RegionList::iterator region_iterator;
  typedef RegionList::const_iterator const_region_iterator;
  typedef UseDefMap::iterator def_iterator;
  typedef UseDefMap::const_iterator const_def_iterator;

 public:
  PMemVariableLocator() {}

  bool runOnFunction(Function &F);
  bool runOnModule(Module &M);

  inline api_iterator call_begin() { return callList.begin(); }
  inline api_iterator call_end() { return callList.end(); }
  inline ApiCallListImpl & calls() { return callList; }

  inline api_const_iterator call_begin() const { return callList.begin(); }
  inline api_const_iterator call_end() const { return callList.end(); }
  inline size_t call_size() const { return callList.size(); }

  inline var_iterator var_begin() { return varList.begin(); }
  inline var_iterator var_end() { return varList.end(); }
  inline VariableListImpl & vars() { return varList; }

  inline const_var_iterator var_begin() const { return varList.begin(); }
  inline const_var_iterator var_end() const { return varList.end(); }
  inline size_t var_size() const { return varList.size(); }

  inline region_iterator region_begin() { return regionList.begin(); }
  inline region_iterator region_end() { return regionList.end(); }

  inline const_region_iterator region_begin() const { return regionList.begin(); }
  inline const_region_iterator region_end() const { return regionList.end(); }
  inline size_t region_size() const { return regionList.size(); }

  inline def_iterator def_begin() { return useDefMap.begin(); }
  inline def_iterator def_end() { return useDefMap.end(); }

  inline const_def_iterator def_begin() const { return useDefMap.begin(); }
  inline const_def_iterator def_end() const { return useDefMap.end(); }
  inline size_t def_size() const { return useDefMap.size(); }

  UseDefMap &def_map() { return useDefMap; }

  def_iterator find_def(llvm::Instruction *inst) { return useDefMap.find(inst); }
  const_def_iterator find_def(llvm::Instruction *inst) const {
    return useDefMap.find(inst);
  }

  static bool callReturnsPmemVar(const char *func_name);

 protected:
  void handleMemKindCall(llvm::CallInst *callInst);
  void handlePmdkCall(llvm::CallInst *callInst);
  bool processDefinitionPoint(llvm::Value *def);

 private:
  ApiCallList callList;
  VariableList varList;
  RegionList regionList;
  UseDefMap useDefMap;
  ProcessedUses processedUses;

  static const std::set<std::string> pmdkApiSet;
  static const std::set<std::string> pmdkPMEMVariableReturnSet;
  static const std::set<std::string> memkindApiSet;
  static const std::set<std::string> memkindVariableReturnSet;
  static const std::map<std::string, unsigned int> pmdkRegionSizeArgMapping;
  static const std::map<std::string, unsigned int> memkindCreationPMEMMapping;
  static const std::map<std::string, unsigned int> memkindCreationGeneralMapping;
};

} // namespace pmem
} // namespace llvm

#endif /* __EXTRACTOR_H_ */
