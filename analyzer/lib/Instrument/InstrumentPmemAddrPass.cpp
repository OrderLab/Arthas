// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "PMem/PMemVariablePass.h"
#include "Instrument/InstrumentPmemAddrPass.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "hook-pmem-addr"

using namespace std;
using namespace llvm;
using namespace llvm::pmem;
using namespace llvm::instrument;

static cl::opt<bool> RegularLoadStore(
    "regular-load-store", cl::desc("Whether to instrument regular load/store"));

static cl::opt<string> HookGuidFile(
    "guid-ouput", cl::desc("File to write the hook GUID map file"),
    cl::value_desc("file"));

void InstrumentPmemAddrPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  // AU.addRequiredTransitive<PMemVariablePass>();
}

bool InstrumentPmemAddrPass::runOnModule(Module &M) {
  bool modified = false;
  _instrumenter = make_unique<PmemAddrInstrumenter>();
  if (!_instrumenter->initHookFuncs(M)) {
    return false;
  }
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
        modified |= runOnFunction(F);
    }
  }
  if (HookGuidFile.empty()) {
    auto & source_file = M.getSourceFileName();
    size_t extindex = source_file.find_last_of(".");
    if (extindex == string::npos) {
      HookGuidFile = source_file + "-hook-guids.map";
    } else {
      HookGuidFile = source_file.substr(0, extindex) + "-hook-guids.map";
    }
    errs() << "Hook map to " << HookGuidFile << "\n";
  }
  errs() << "Instrumented " << _instrumenter->getInstrumentedCnt();
  if (RegularLoadStore) {
    errs() << " regular load/store instructions in total\n";
  } else {
    errs() << " pmem instructions in total\n";
  }
  _instrumenter->writeGuidHookPointMap(HookGuidFile);
  errs() << "The hook GUID map is written to " << HookGuidFile << "\n";
  return modified;
}

bool InstrumentPmemAddrPass::runOnFunction(Function &F) 
{
  bool instrumented = false;
  if (RegularLoadStore) {
    // instrumenting regular load-store instructions, this is useful for testing
    // purposes, e.g., instrumenting the loop1.c test case
    Instruction *instr;
    for (inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ++ii) {
      instr = &*ii;
      instrumented |= _instrumenter->instrumentInstr(instr);
    }
  } else {
    pmem::PMemVariableLocator locator;
    locator.runOnFunction(F);
    if (locator.var_size() == 0) {
      DEBUG(dbgs() << "No pmem instructions found in " << F.getName()
          << ", skip instrumentation\n");
      return false;
    }
    for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
      Value *var = *vi;
      if (Instruction *instr = dyn_cast<Instruction>(var)) {
        if (_instrumenter->instrumentInstr(instr)) {
          instrumented = true;
          DEBUG(dbgs() << "Instrumented pmem var instruction in " << F.getName()
                       << ":" << *instr << "\n");
        }
      }
    }
    for (auto ri = locator.region_begin(); ri != locator.region_end(); ++ri) {
      Value *region = ri->first;
      if (Instruction *instr = dyn_cast<Instruction>(region)) {
        if (_instrumenter->instrumentInstr(instr)) {
          instrumented = true;
          DEBUG(dbgs() << "Instrumented pmem region instruction in "
                       << F.getName() << ":" << *instr << "\n");
        }
      }
    }
  }
  return instrumented;
}

char InstrumentPmemAddrPass::ID = 0;
static RegisterPass<InstrumentPmemAddrPass> X(
    "instr", "Instruments the PMem address related code");
