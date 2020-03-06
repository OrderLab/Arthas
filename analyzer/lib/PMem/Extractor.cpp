// The Arthas Project
//
// Created by ryanhuang on 12/24/19.
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <set>

#include "DefUse/DefUse.h"
#include "PMem/Extractor.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#include "dg/llvm/LLVMDependenceGraph.h"
#include "dg/llvm/LLVMDependenceGraphBuilder.h"
#include "dg/llvm/LLVMDependenceGraphBuilder.h"
#include "dg/llvm/LLVMNode.h"
#include "dg/llvm/LLVMSlicer.h"
#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"

#include "dg/analysis/PointsTo/PointerAnalysisFI.h"
#include "dg/analysis/PointsTo/PointerAnalysisFS.h"
#include "dg/analysis/PointsTo/Pointer.h"

#include "dg/util/TimeMeasure.h"

using namespace std;
using namespace llvm;
using namespace llvm::pmem;
using namespace llvm::defuse;

#define DEBUG_TYPE "pmem-extractor"

const set<std::string> PMemVariableLocator::pmdkApiSet{
    "pmem_persist",          "pmem_msync",   "pmemobj_create",
    "pmemobj_direct_inline", "pmemobj_open", "pmem_map_file"};

const set<std::string> PMemVariableLocator::pmdkPMEMVariableReturnSet{
    "pmemobj_direct_inline", "pmem_map_file"};

const set<std::string> PMemVariableLocator::memkindApiSet{
    "memkind_create_pmem",   "memkind_create_kind"};

const set<std::string> PMemVariableLocator::memkindVariableReturnSet{
    "memkind_malloc",        "memkind_calloc"};

// Map API name to i-th argument (starting from 0) that specifies region size
const map<std::string, unsigned int> PMemVariableLocator::pmdkRegionSizeArgMapping{
  {"pmem_map_file", 1}, {"pmemobj_create", 2}};

const map<std::string, unsigned int> PMemVariableLocator::memkindCreationPMEMMapping{
  {"memkind_create_pmem", 3}};

const map<std::string, unsigned int> PMemVariableLocator::memkindCreationGeneralMapping{
  {"memkind_create_kind", 4}};

bool PMemVariableLocator::runOnModule(Module &M) 
{
  bool modified = false;
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    if (F.isDeclaration()) continue;
    modified |= runOnFunction(F);
  }
  return modified;
}

bool PMemVariableLocator::runOnFunction(Function &F) 
{
  if (F.isDeclaration()) {
    return false;
  }
  DEBUG(dbgs() << "[" << F.getName() << "]\n");
  for (inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ++I) {
    Instruction *inst = &*I;
    if (!isa<CallInst>(inst)) continue;
    CallInst *callInst = cast<CallInst>(inst);
    handlePmdkCall(callInst);
    handleMemKindCall(callInst);
  }
  for (Value *def : varList) {
    processDefinitionPoint(def);
  }
  for (auto &entry : useDefMap) {
    errs() << "Definition point for '" << *entry.first << "':\n";
    for (Value *def : entry.second) {
      errs() << "-->'" << *def << "'\n";
    }
  }
  return false;
}

void PMemVariableLocator::handleMemKindCall(CallInst *callInst)
{
    Function *callee = callInst->getCalledFunction();
    if (!callee) return;

    // Step 1: Check for Memkind API call instructions
    //
    //  Different behavior than PMDK because memkind_malloc can be used for non 
    //  Persistent Memory behavior
    // TODO: add checks for Memkind_malloc for PMEM vs volatile
    if (memkindApiSet.find(callee->getName()) != memkindApiSet.end()) {
      auto rit = memkindCreationPMEMMapping.find(callee->getName());
      if (rit != memkindCreationPMEMMapping.end() &&
          callInst->getNumArgOperands() >= rit->second + 1) {
        // check if the call instruction has the right number of arguments
        // +1 as the mapping stores the target argument from 0.

        // find the argument that corresponds to memkind persistent variable
        Value *v = callInst->getArgOperand(rit->second);
        varList.push_back(v);
        // Transitive closure to see which memkind_malloc calls use this memkind
        // pmem variable
        UserGraph g(v);
        for (auto ui = g.begin(); ui != g.end(); ++ui) {
          DEBUG(dbgs() << "- users of the pmem variable: " << *(ui->first)
                       << "\n");
          varList.push_back(ui->first);
        }
      }
      // Add check for memkind_create_kind and see if that uses pmem
      // or non pmem type
      auto rit2 = memkindCreationGeneralMapping.find(callee->getName());
      if (rit2 != memkindCreationGeneralMapping.end() &&
          callInst->getNumArgOperands() >= rit2->second + 1) {
        // find the argument that corresponds to memkind persistent variable
        Value *v = callInst->getArgOperand(rit2->second);
        // Check if argument is MEMKIND_DAX_PMEM
        if (v->getName().compare("MEMKIND_DAX_PMEM") == 0) {
          varList.push_back(v);
          // Transitive closure to see which memkind_malloc calls use this
          // memkind pmem var$
          UserGraph g(v);
          for (auto ui = g.begin(); ui != g.end(); ++ui) {
            DEBUG(dbgs() << "- users of the pmem variable: " << *(ui->first) << "\n");
            varList.push_back(ui->first);
          }
        }
      }
  }
}

void PMemVariableLocator::handlePmdkCall(CallInst *callInst)
{
  Function *callee = callInst->getCalledFunction();
  if (!callee) return;
  // Step 1: Check for PMDK API call instructions 
  if (pmdkApiSet.find(callee->getName()) != pmdkApiSet.end()) {
    callList.push_back(callInst);
    if (pmdkPMEMVariableReturnSet.find(callee->getName()) !=
        pmdkPMEMVariableReturnSet.end()) {
      // Step 2: if this API call returns something, we get a pmem variable.
      Value *v = callInst;
      DEBUG(dbgs() << "- this instruction creates a pmem variable: " << *v
                   << "\n");
      varList.push_back(v);
      // Step 3: find the transitive closure of the users of the pmem variables.
      UserGraph g(v);
      for (auto ui = g.begin(); ui != g.end(); ++ui) {
        DEBUG(dbgs() << "- users of the pmem variable: " << *(ui->first) << "\n");
        varList.push_back(ui->first);
      }
    }
    // Step 4: Find persistent memory regions (e.g., mmapped through pmem_map_file 
    // call) and their size argument to check all pointers if they point to a PMEM region.
    auto rit = pmdkRegionSizeArgMapping.find(callee->getName());
    if (rit != pmdkRegionSizeArgMapping.end() && 
        callInst->getNumArgOperands() >= rit->second + 1) { 
      // check if the call instruction has the right number of arguments
      // +1 as the mapping stores the target argument from 0.

      // find the argument that specifies the object store or mmap region size.
      regionList.insert(RegionInfo(callInst, callInst->getArgOperand(rit->second)));
    }
  }
}

bool PMemVariableLocator::processDefinitionPoint(llvm::Value *def) {
  if (processedUses.find(def) != processedUses.end()) {
    errs() << "Definition point " << *def << " was processed before\n";
    return false;
  }

  // FIXME: make the reaching definition more accurate.
  //
  // the user graph stores transitive closure of uses, but we need
  // the *reaching* definition for a user. the definition points we get can
  // contain superfluous loads or killed definitions.
  //
  // For example, consider the 'strcpy' call instruction in the
  // write_hello_string function of the hello_libpmem.c test case, i.e.,
  //
  //    %16 = call i8* @strcpy(i8* %14, i8* %15) #8, !dbg !39
  //
  // The identified definition points for this instruction consists of two Values:
  //
  //    %10 = call i8* @pmem_map_file(i8* %9, i64 1024, i32 1, i32 438, i64* %6, i32* %7), !dbg !30'
  //    %14 = load i8*, i8** %5, align 8, !dbg !37
  //
  // Ideally, we want the first one. The second one is unnecessary as the memory
  // content pointed by %5 comes from (stored) %10.
  
  UserGraph g(def);
  for (auto ui = g.begin(); ui != g.end(); ++ui) {
    Value *u = ui->first;
    if (Instruction *inst = dyn_cast<Instruction>(u)) {
      auto di = useDefMap.find(inst);
      if (di == useDefMap.end()) {
        useDefMap.emplace(inst, DefinitionPoints{def});
      } else {
        di->second.insert(def);
      }
    }
  }
  processedUses.insert(def);
  return true;
}
