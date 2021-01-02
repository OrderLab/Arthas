// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <fstream>
#include <iostream>

#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/CommandLine.h>

#include "Instrument/InstrumentPmemAddrPass.h"
#include "PMem/PMemVariablePass.h"
#include "Utils/LLVM.h"
#include "Utils/String.h"

#include "PMem/Extractor.h"
#include "Slicing/SliceCriteria.h"
#include "Slicing/Slicer.h"
#include "dg/ADT/Queue.h"
#include "dg/analysis/ReachingDefinitions/ReachingDefinitions.h"
#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"
#include "dg/llvm/analysis/PointsTo/PointerGraph.h"

using namespace std;
using namespace llvm;
using namespace llvm::pmem;
using namespace llvm::instrument;
using namespace llvm::slicing;

set<llvm::Value *> pmem_vars;

cl::opt<string> inputFilename(cl::Positional, cl::desc("<input file>"),
                              cl::Required);
cl::opt<string> outputFilename(
    "o", cl::desc("File to write the instrumented bitcode"),
    cl::value_desc("file"));
cl::opt<bool> RegularLoadStore(
    "regular-load-store", cl::desc("Whether to instrument regular load/store"));

cl::opt<string> HookGuidFile("guid-ouput",
                             cl::desc("File to write the hook GUID map file"),
                             cl::value_desc("file"));

void instrumentPmemPointers(Function *F, dg::LLVMPointerAnalysis *pta,
                            dg::LLVMDependenceGraph *dep_graph,
                            dg::LLVMReachingDefinitions *rda,
                            PmemAddrInstrumenter *instrumenter) {
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    Instruction *inst = &*I;
    Value *val = dyn_cast<Value>(inst);
    auto ps = pta->getPointsTo(val);
    auto rd = rda->getNode(val);
    if (!ps) {
      continue;
    }
    for (const auto &ptr : ps->pointsTo) {
      Value *p_val = ptr.target->getUserData<Value>();
      if (!p_val) continue;
      // std::cout << "P_VAL!!!\n";
      // llvm::errs() << *p_val << "\n";
      for (auto it = pmem_vars.begin(); it != pmem_vars.end(); it++) {
        Value *pmem_val = *it;
        if (p_val == pmem_val) {
          pmem_vars.insert(val);
          llvm::errs() << "we are inserting " << *val << "\n";
          break;
        }
      }
    }
  }
}

bool runOnFunction(PmemAddrInstrumenter *instrumenter, Function &F) {
  bool instrumented = false;
  if (RegularLoadStore) {
    // instrumenting regular load-store instructions, this is useful for testing
    // purposes, e.g., instrumenting the loop1.c test case
    Instruction *instr;
    for (inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ++ii) {
      instr = &*ii;
      instrumented |= instrumenter->instrumentInstr(instr);
    }
  } else {
    // if(low_level){}
    // Instrumenting every instruction for low level purposes
    /*Instruction *instr;
    for (inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ++ii) {
      instr = &*ii;
      instrumented |= instrumenter->instrumentInstr(instr);
      errs() << "Instruction is " << *instr << "\n";
    }*/
    PMemVariableLocator locator;
    locator.runOnFunction(F);
    if (locator.var_size() == 0) {
      return false;
    }
    errs() << F.getName() << "\n";
    for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
      Value *var = *vi;
      errs() << *var << "\n";
      pmem_vars.insert(var);
      instrumented = true;
      // Comment this out if doing low-level
      if (Instruction *instr = dyn_cast<Instruction>(var)) {
        if (instrumenter->instrumentInstr(instr)) {
          instrumented = true;
        }
      }
    }
    for (auto ri = locator.region_begin(); ri != locator.region_end(); ++ri) {
      Value *region = ri->first;
      pmem_vars.insert(region);
      instrumented = true;
      if (Instruction *instr = dyn_cast<Instruction>(region)) {
        if (instrumenter->instrumentInstr(instr)) {
          instrumented = true;
        }
      }
    }
  }
  return instrumented;
}

bool saveModule(Module *M, string outFile) {
  verifyModule(*M, &errs());
  ofstream ofs(outFile);
  raw_os_ostream ostream(ofs);
  WriteBitcodeToFile(M, ostream);
  return true;
}

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv);

  LLVMContext context;
  unique_ptr<Module> M = parseModule(context, inputFilename);
  if (!M) {
    errs() << "Failed to parse '" << inputFilename << "' file:\n";
    return 1;
  }
  PmemAddrInstrumenter instrumenter;
  if (!instrumenter.initHookFuncs(*M)) {
    errs() << "Failed to initialize hook functions\n";
    return 1;
  }
  set<Function *> Functions;
  for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
      runOnFunction(&instrumenter, F);
      Functions.insert(&F);
    }
  }

  cout << "first iteration " << pmem_vars.size() << "\n";
  // Create Dg Slice Graph here
  // unique_ptr<Module> M2 = parseModule(context, inputFilename);
  // Module *M2 = M.get();
  // auto M3 = make_unique<Module>(M2);
  // M = move(M3);
  std::clock_t time_start = clock();
  auto slicer = make_unique<DgSlicer>(M.get(), SliceDirection::Full);
  uint32_t flags = SlicerDgFlags::ENABLE_PTA | SlicerDgFlags::INTER_PROCEDURAL;
  SlicerDgFlags::ENABLE_CONTROL_DEP;
  Function *entry = nullptr;
  auto options = slicer->createDgOptions(flags);
  slicer->computeDependencies(options);
  llvm::errs() << "done with slice creation\n";
  std::clock_t time_end = clock();
  llvm::errs() << "INFO: Created dependency grpah in "
               << 1000.0 * (time_end - time_start) / CLOCKS_PER_SEC << "\n";

  // Second iteration to get pointer analysis PMEM Variables
  Function *instr_fun;
  for (auto it = Functions.begin(); it != Functions.end(); it++) {
    Function *F = *it;
    if (!F->isDeclaration()) {
      dg::LLVMDependenceGraph *dep_graph = slicer->getDependenceGraph(F);
      if (!dep_graph) continue;
      errs() << "Extracting pmem pointers from " << F->getName() << "()\n";
      dg::LLVMPointerAnalysis *pta = dep_graph->getPTA();
      dg::LLVMReachingDefinitions *rda = dep_graph->getRDA();
      if (!pta || !rda) {
        errs() << "pta or rda is NULL\n";
        continue;
      }
      instrumentPmemPointers(F, pta, dep_graph, rda, &instrumenter);
    }
  }
  int instrument_count = 0;
  // Instrument pmem variables
  for (auto it = pmem_vars.begin(); it != pmem_vars.end(); it++) {
    Value *var = *it;
    llvm::errs() << "instrument var " << *var << "\n";
    instrument_count++;
    if (Instruction *instr = dyn_cast<Instruction>(var)) {
      instr_fun = instr->getFunction();
      errs() << "CHECKING FOR: " << instr_fun->getName() << "\n";
      if (instr_fun->getName().compare("split_slab_page_into_freelist") == 0) {
        // don't instrument
      } else if (instr_fun->getName().compare("do_slabs_free") == 0) {
        // don't instrument
      } else if (instrumenter.instrumentInstr(instr)) {
      }
      /*if (instrumenter.instrumentInstr(instr)) {
      }*/
    }
  }
  llvm::errs() << "INSTRUMENTED " << instrument_count << "\n";

  string inputFileBasenameNoExt = getFileBaseName(inputFilename, false);
  if (outputFilename.empty()) {
    outputFilename = inputFileBasenameNoExt + "-instrumented.bc";
  }
  if (!saveModule(M.get(), outputFilename)) {
    errs() << "Failed to save the instrumented bitcode file to "
           << outputFilename << "\n";
    return 1;
  }
  errs() << "Saved the instrumented bitcode file to " << outputFilename << "\n";
  if (HookGuidFile.empty()) {
    HookGuidFile = inputFileBasenameNoExt + "-hook-guids.map";
    errs() << "Hook map to " << HookGuidFile << "\n";
  }
  errs() << "Instrumented " << instrumenter.getInstrumentedCnt();
  if (RegularLoadStore) {
    errs() << " regular load/store instructions in total\n";
  } else {
    errs() << " pmem instructions in total\n";
  }
  instrumenter.writeGuidHookPointMap(HookGuidFile);
  errs() << "The hook GUID map is written to " << HookGuidFile << "\n";
  return 0;
}
