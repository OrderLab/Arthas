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

static cl::opt<string> HookGuidFile(
    "guid-ouput", cl::desc("File to write the hook GUID map file"),
    cl::init("hook_guids.dat"), cl::value_desc("file"));

void InstrumentPmemAddrPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  // AU.addRequiredTransitive<PMemVariablePass>();
}

bool InstrumentPmemAddrPass::runOnModule(Module &M) {
  bool modified = false;
  instrumenter = make_unique<PmemAddrInstrumenter>();
  if (!instrumenter->initHookFuncs(M)) return false;
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
        modified |= runOnFunction(F);
    }
  }
  instrumenter->writeGuidHookPointMap(HookGuidFile);
  errs() << "Instrumented " << instrumented << " pmem instructions in total\n";
  return modified;
}

bool InstrumentPmemAddrPass::runOnFunction(Function &F) 
{
  pmem::PMemVariableLocator locator;
  locator.runOnFunction(F);
  if (locator.var_size() == 0) {
    DEBUG(dbgs() << "No pmem instructions found in " << F.getName() 
        << ", skip instrumentation\n");
    return false;
  }
  for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
    Value *v = *vi;
    if (Instruction *instr = dyn_cast<Instruction>(v)) {
      if (instrumenter->instrumentInstr(instr)) {
        DEBUG(dbgs() << "Instrumented pmem instruction in " << F.getName() 
            << ":" << *instr << "\n");
        instrumented++;
      }
    }
  }
  return true;
}

char InstrumentPmemAddrPass::ID = 0;
static RegisterPass<InstrumentPmemAddrPass> X(
    "instr", "Instruments the PMem address related code");
