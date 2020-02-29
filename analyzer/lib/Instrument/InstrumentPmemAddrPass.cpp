// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Instrument/PmemAddrInstrumenter.h"

#include "llvm/Support/CommandLine.h"

using namespace std;
using namespace llvm;
using namespace llvm::pmem;
using namespace llvm::instrument;

static cl::opt<string> HookGuidFile(
    "guid-ouput", cl::desc("File to write the hook GUID map file"),
    cl::init("hook_guids.dat"), cl::value_desc("file"));

namespace {
class InstrumentPmemAddrPass: public ModulePass {
 public:
  static char ID;
  InstrumentPmemAddrPass() : ModulePass(ID) {}

  bool runOnModule(Module &M);
  bool runOnFunction(Function &F);

 protected:
  std::unique_ptr<PmemAddrInstrumenter> instrumenter;
};
}

bool InstrumentPmemAddrPass::runOnModule(Module &M) 
{
  instrumenter = make_unique<PmemAddrInstrumenter>();
  if (!instrumenter->initHookFuncs(M)) return false;
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    runOnFunction(F);
  }
  instrumenter->dumpHookGuidMapToFile(HookGuidFile);
  return false;
}

bool InstrumentPmemAddrPass::runOnFunction(Function &F)
{
  Instruction *instr;
  for (inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ++ii) {
    instr = &*ii;
    //TODO: modify to only include instructions in Slices, get locator to get Pmem variables
    //get slicer, find overlap in slices, pmem variables
    // pmem::PMemVariableLocator locator(F);
    instrumenter->instrumentInstr(instr);
  }
  return false;
}

char InstrumentPmemAddrPass::ID = 0;
static RegisterPass<InstrumentPmemAddrPass> X(
    "instr", "Instruments the PMem address related code");
