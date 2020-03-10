// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <fstream>
#include <iostream>
#include <vector>
#include <map>

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"

#include "DefUse/DefUse.h"
#include "Instrument/PmemAddrTrace.h"
#include "Instrument/PmemVarGuidMap.h"
#include "Matcher/Matcher.h"
#include "Slicing/Slice.h"
#include "Slicing/SliceCriteria.h"
#include "Slicing/Slicer.h"
#include "Utils/LLVM.h"
#include "Utils/String.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::instrument;
using namespace llvm::defuse;
using namespace llvm::matching;

cl::opt<string> inputFilename(cl::Positional, cl::desc("<input file>"),
                              cl::Required);

cl::opt<string> faultInst("i", cl::desc("Instruction to start slicing"),
                          cl::value_desc("instruction"));

cl::opt<string> faultInstLocation(
    "l", cl::desc("Fault instruction location information"),
    cl::value_desc("file:line[:function]"));

cl::opt<string> pmemHookGuidFile(
    "g", cl::desc("File to the pmem hook GUID map file"),
    cl::value_desc("file"));

cl::opt<string> addrTraceFile(
    "a", cl::desc("File to the dynamic address trace file"),
    cl::value_desc("file"));

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv);

  LLVMContext context;
  unique_ptr<Module> M = parseModule(context, inputFilename);
  if (!M) {
    errs() << "Failed to parse '" << inputFilename << "' file:\n";
    return 1;
  }
  errs() << "Successfully parsed " << inputFilename << "\n";

  PmemVarGuidMap varMap;
  if (!PmemVarGuidMap::deserialize(pmemHookGuidFile.c_str(), varMap)) {
    errs() << "Failed to parse hook GUID file " << pmemHookGuidFile << "\n";
    return 1;
  }
  errs() << "Successfully parsed hook guid map with " << varMap.size()
         << " entries\n";

  PmemAddrTrace addrTrace;
  if (!PmemAddrTrace::deserialize(addrTraceFile.c_str(), &varMap, addrTrace)) {
    errs() << "Failed to parse hook GUID file " << addrTraceFile << "\n";
    return 1;
  }
  errs() << "Successfully parsed " << addrTrace.size()
         << " dynamic address trace items\n";

  if (addrTrace.pool_empty()) {
    errs() << "No pool address found in the address trace file, abort\n";
    return 1;
  }

  if (!addrTrace.calculatePoolOffsets()) {
    errs() << "Failed to calculate the address offsets w.r.t the pool address "
              "in the address trace file, abort\n";
    return 1;
  }

  // map address to instructions
  Matcher matcher;
  matcher.process(*M);
  addrTrace.addressesToInstructions(&matcher);
}
