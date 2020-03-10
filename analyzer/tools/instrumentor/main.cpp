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

using namespace std;
using namespace llvm;
using namespace llvm::pmem;
using namespace llvm::instrument;

cl::opt<string> inputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);
cl::opt<string> outputFilename(
    "o", cl::desc("File to write the instrumented bitcode"),
    cl::value_desc("file"));
cl::opt<bool> RegularLoadStore(
    "regular-load-store", cl::desc("Whether to instrument regular load/store"));

cl::opt<string> HookGuidFile("guid-ouput",
                             cl::desc("File to write the hook GUID map file"),
                             cl::value_desc("file"));

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
    pmem::PMemVariableLocator locator;
    locator.runOnFunction(F);
    if (locator.var_size() == 0) {
      return false;
    }
    for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
      Value *var = *vi;
      if (Instruction *instr = dyn_cast<Instruction>(var)) {
        if (instrumenter->instrumentInstr(instr)) {
          instrumented = true;
        }
      }
    }
    for (auto ri = locator.region_begin(); ri != locator.region_end(); ++ri) {
      Value *region = ri->first;
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
  for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
      runOnFunction(&instrumenter, F);
    }
  }

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
