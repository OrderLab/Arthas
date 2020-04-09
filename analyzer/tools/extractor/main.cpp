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

cl::list<std::string> TargetFunctions("function", cl::desc("<Function>"),
                                      cl::ZeroOrMore);
cl::opt<string> inputFilename(cl::Positional, cl::desc("<input file>"),
                              cl::Required);

void extractPmemPointers(Function *F, dg::LLVMPointerAnalysis *pta) {
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    Instruction *inst = &*I;
    Value *val = dyn_cast<Value>(inst);
    auto ps = pta->getPointsTo(val);
    for (const auto &ptr : ps->pointsTo) {
      // TODO: either leverage UsesTheVariable Function in llvm-slicer-crit.cpp
      // or find a way to get value from ptr node and compare to pmem_vars.
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
  for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
      if (targetFunctionSet.empty() ||
          targetFunctionSet.count(F.getName()) != 0) {
        errs() << "Extracting pmem variables from " << F.getName() << "()\n";
        extractPmemVarInFunc(&F);
      }
    }
  }
  // Create Dg Slice Graph here
  auto slicer = make_unique<DgSlicer>(M.release(), SliceDirection::Backward);
  uint32_t flags = SlicerDgFlags::ENABLE_PTA | SlicerDgFlags::INTER_PROCEDURAL;
  Function *entry = nullptr;
  auto options = slicer->createDgOptions(flags);
  slicer->computeDependencies(options);

  // Second iteration across Functions that gets PTA
  for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
      if (targetFunctionSet.empty() ||
          targetFunctionSet.count(F.getName()) != 0) {
        errs() << "Extracting pmem pointers from " << F.getName() << "()\n";
        dg::LLVMDependenceGraph *dep_graph = slicer->getDependenceGraph(&F);
        dg::LLVMPointerAnalysis *pta = dep_graph->getPTA();
        // TODO: Using pta and PointsTo set, use this function to find pmem
        // variables
        extractPmemPointers(&F, pta);
      }
    }
  }
}
