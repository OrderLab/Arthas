// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <iostream>

#include <llvm/Support/CommandLine.h>

#include "PMem/Extractor.h"
#include "PMem/PMemVariablePass.h"
#include "Slicing/SliceCriteria.h"
#include "Slicing/Slicer.h"
#include "Utils/LLVM.h"
#include "Utils/String.h"

#include "dg/ADT/Queue.h"
#include "dg/analysis/ReachingDefinitions/ReachingDefinitions.h"
#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"
#include "dg/llvm/analysis/PointsTo/PointerGraph.h"

using namespace std;
using namespace llvm;
using namespace llvm::pmem;
using namespace llvm::slicing;

set<llvm::Value *> pmem_vars;
int insert_count = 0;
int instruction_count = 0;

cl::list<std::string> TargetFunctions("function", cl::desc("<Function>"),
                                      cl::ZeroOrMore);
cl::opt<string> inputFilename(cl::Positional, cl::desc("<input file>"),
                              cl::Required);

void extractPmemPointers(Function *F, dg::LLVMPointerAnalysis *pta,
                         dg::LLVMDependenceGraph *dep_graph,
                         dg::LLVMReachingDefinitions *rda) {
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    Instruction *inst = &*I;
    Value *val = dyn_cast<Value>(inst);
    instruction_count++;
    auto ps = pta->getPointsTo(val);
    if (!ps) {
      continue;
    }
    for (const auto &ptr : ps->pointsTo) {
      Value *p_val = ptr.target->getUserData<Value>();
      if (!p_val) continue;
      for (auto it = pmem_vars.begin(); it != pmem_vars.end(); it++) {
        Value *pmem_val = *it;
        if (p_val == pmem_val) {
          pmem_vars.insert(val);
          insert_count += 1;
          break;
        }
      }
    }
  }
}

void extractPmemVarInFunc(Function *F) {
  PMemVariableLocator locator;
  locator.runOnFunction(*F);
  // Iterate through the identified PMDK API calls in this function
  for (auto ci = locator.call_begin(); ci != locator.call_end(); ++ci) {
    errs() << "* Identified pmdk API calls: ";
    const CallInst *inst = *ci;
    inst->dump();
  }

  // Iterate through the identified PMem variables in this function
  for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
    errs() << "* Identified pmem variable instruction: " << **vi << "\n";
    pmem_vars.insert(*vi);
  }

  // Iterate through the identified PMem ranges in this function
  for (auto ri = locator.region_begin(); ri != locator.region_end(); ++ri) {
    errs() << "* Identified pmem region [" << *ri->first << "," << *ri->second
           << "]\n";
  }
}

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv);

  LLVMContext context;
  unique_ptr<Module> M = parseModule(context, inputFilename);
  if (!M) {
    errs() << "Failed to parse '" << inputFilename << "' file:\n";
    return 1;
  }
  set<string> targetFunctionSet(TargetFunctions.begin(), TargetFunctions.end());
  set<Function *> Functions;
  for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
      if (targetFunctionSet.empty() ||
          targetFunctionSet.count(F.getName()) != 0) {
        errs() << "Extracting pmem variables from " << F.getName() << "()\n";
        extractPmemVarInFunc(&F);
        Functions.insert(&F);
      }
    }
  }
  // Create Dg Slice Graph here
  auto slicer = make_unique<DgSlicer>(M.release(), SliceDirection::Full);
  uint32_t flags = SlicerDgFlags::ENABLE_PTA | SlicerDgFlags::INTER_PROCEDURAL;
  //SlicerDgFlags::ENABLE_CONTROL_DEP;
  auto options = slicer->createDgOptions(flags);
  slicer->computeDependencies(options);
  llvm::errs() << "done with slice creation\n";

  // Second iteration across Functions that gets PTA
  unique_ptr<Module> Mt = parseModule(context, inputFilename);
  for (auto it = Functions.begin(); it != Functions.end(); it++) {
    Function *F = *it;
    if (!F->isDeclaration()) {
      if (targetFunctionSet.empty() ||
          targetFunctionSet.count(F->getName()) != 0) {
        dg::LLVMDependenceGraph *dep_graph = slicer->getDependenceGraph(F);
        if (!dep_graph) continue;
        errs() << "Extracting pmem pointers from " << F->getName() << "()\n";
        dg::LLVMPointerAnalysis *pta = dep_graph->getPTA();
        dg::LLVMReachingDefinitions *rda = dep_graph->getRDA();
        if (!pta || !rda) {
          errs() << "pta or rda is NULL\n";
          continue;
        }
        extractPmemPointers(F, pta, dep_graph, rda);
      }
    }
  }
  cout << "insert count at end is " << insert_count << "\n";
  cout << "instruction count at end is " << instruction_count << "\n";
}
