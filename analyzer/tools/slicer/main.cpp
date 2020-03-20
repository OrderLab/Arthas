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

#include "llvm/IR/InstIterator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"

#include "DefUse/DefUse.h"
#include "Instrument/PmemAddrInstrumenter.h"
#include "Matcher/Matcher.h"
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

cl::opt<string> inputFilename(cl::Positional, cl::desc("<input file>"),
                              cl::Required);

cl::opt<string> sliceOutput("slice-file",
                            cl::desc("File to write the slice output to"),
                            cl::init("slices.log"), cl::value_desc("file"));

cl::opt<string> sliceFunc("func", cl::desc("Function where the slicing starts"),
                          cl::value_desc("function"));

cl::opt<string> sliceInst("inst", cl::desc("Instruction to start slicing"),
                          cl::value_desc("instruction"));

cl::opt<int> sliceInstNo(
    "inst-no", cl::desc("Nth instruction in a function to start slicing"),
    cl::init(0), cl::value_desc("instruction"));

cl::opt<SliceDirection> sliceDir(
    "dir", cl::desc("Slicing direction"),
    cl::values(
        clEnumValN(SliceDirection::Backward, "backward", "Backward slicing"),
        clEnumValN(SliceDirection::Forward, "forward", "Forward slicing"),
        clEnumValN(SliceDirection::Full, "full", "Full slicing"), clEnumValEnd),
    cl::init(SliceDirection::Backward));

cl::opt<string> sliceCriteria(
    "criteria", cl::desc("Comma separated list of slicing criterion"),
    cl::value_desc("file1:line1,file2:line2,..."));

cl::opt<bool> instrumentPmemSlice(
    "hook-pmem",
    cl::desc("Whether to instrument the pmem variable in a slice"));

cl::opt<string> instrumentOutput(
    "output", cl::desc("Output file name of the instrumented program"));

cl::opt<string> pmemHookGuidFile(
    "hook-guid-ouput", cl::desc("File to write the pmem hook GUID map file"),
    cl::init("hook_guids.dat"), cl::value_desc("file"));

cl::opt<bool> enablePTA("pta", cl::desc("Enabling pointer analysis"),
                        cl::init(true));
cl::opt<bool> enableCtrl("ctrl",
                         cl::desc("Enabling control dependency analysis"),
                         cl::init(false));
cl::opt<bool> enableThd("thread",
                        cl::desc("Enabling thread support in analysis"),
                        cl::init(true));
cl::opt<bool> intraProcedural("intra",
                              cl::desc("Enabling intra-procedural analysis"),
                              cl::init(false));
cl::opt<bool> interProcedural("inter",
                              cl::desc("Enabling inter-procedural analysis"),
                              cl::init(true));

void instructionSlice(DgSlicer *slicer, Instruction *fault_inst,
                      PMemVariableLocator &locator, Slices &slices,
                      raw_ostream &out_stream) {
  uint32_t slice_id = 0;
  SliceGraph *sg = slicer->slice(fault_inst, slice_id, SlicingApproachKind::Storing);
  if (sg == nullptr) {
    return;
  }
  unique_ptr<SliceGraph> slice_graph(sg);
  auto &st = slicer->getStatistics();
  errs() << "INFO: Sliced away " << st.nodesRemoved << " from " << st.nodesTotal
         << " nodes\n";
  errs() << "INFO: Slice graph has " << slice_graph->size() << " node(s)\n";
  out_stream << "=================Slice graph " << slice_graph->slice_id();
  out_stream << "=================\n";
  out_stream << *slice_graph.get() << "\n";
  errs() << "Slice graph is written to " << sliceOutput << "\n";

  slice_graph->computeSlices(slices);
  out_stream << "=================Slice list " << slice_graph->slice_id();
  out_stream << "=================\n";
  for (Slice *slice : slices) {
    auto persistent_vars = locator.vars().getArrayRef();
    slice->setPersistence(persistent_vars);
    slice->dump(out_stream);
  }
  errs() << "The list of slices are written to " << sliceOutput << "\n";
}

uint32_t createDgFlags() {
  uint32_t flags = 0;
  if (enablePTA) flags |= SlicerDgFlags::ENABLE_PTA;
  if (enableCtrl) flags |= SlicerDgFlags::ENABLE_CONTROL_DEP;
  if (enableThd) flags |= SlicerDgFlags::SUPPORT_THREADS;
  if (intraProcedural) flags |= SlicerDgFlags::INTRA_PROCEDURAL;
  if (interProcedural) flags |= SlicerDgFlags::INTER_PROCEDURAL;
  return flags;
}

bool slice(Module *M, vector<Instruction *> &startInstrs)
{
  errs() << "Begin instruction slice\n";
  auto slicer = make_unique<DgSlicer>(M, sliceDir);
  // enabling pointer analysis, inter-procedural analysis and threading support
  // control dependency analysis is disabled
  uint32_t flags = createDgFlags();
  auto options = slicer->createDgOptions(flags);
  slicer->computeDependencies(options);

  std::error_code ec;
  raw_fd_ostream out_stream(sliceOutput, ec, sys::fs::F_Text);
  unique_ptr<PmemAddrInstrumenter> instrumenter;

  bool instrumented = false;
  if (instrumentPmemSlice) {
    instrumenter = make_unique<PmemAddrInstrumenter>();
    if (!instrumenter->initHookFuncs(*M)) {
      return false;
    }
  }
  map<Function *, unique_ptr<PMemVariableLocator>> locatorMap;
  for (auto fault_inst : startInstrs) {
    Function *F = fault_inst->getFunction();
    auto li = locatorMap.find(F);
    if (li == locatorMap.end()) {
      locatorMap.insert(make_pair(F, make_unique<PMemVariableLocator>()));
    }
    PMemVariableLocator *locator = locatorMap[F].get();
    locator->runOnFunction(*F);
    Slices slices;
    instructionSlice(slicer.get(), fault_inst, *locator, slices, out_stream);
    if (instrumentPmemSlice) {
      for (Slice *slice : slices) {
        if (slice->persistence == SlicePersistence::Volatile) {
          errs() << "Slice " << slice->id << " is volatile, do nothing\n";
        } else {
          errs() << "Slice " << slice->id << " is persistent or mixed, instrument it\n";
          instrumented |= instrumenter->instrumentSlice(slice, locator->def_map());
        }
      }
    }
  }
  if (instrumentPmemSlice) {
    instrumenter->writeGuidHookPointMap(pmemHookGuidFile);
    errs() << "Instrumented " << instrumented << " pmem instructions in total\n";
  }

  if (!instrumentOutput.empty()) {
    ofstream ofs(instrumentOutput);
    raw_os_ostream ostream(ofs);
    WriteBitcodeToFile(M, ostream);
    errs () << "Instrumented program is written into bitcode file " << instrumentOutput << "\n";
  }
  return instrumented;
}

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv);

  LLVMContext context;
  unique_ptr<Module> M = parseModule(context, inputFilename);
  if (!M) {
    errs() << "Failed to parse '" << inputFilename << "' file:\n";
    return 1;
  }
  errs() << "Successfully parsed " << inputFilename << "\n";
  // enable fuzzy matching and ignoring !dbg if necessary
  SliceInstCriteriaOpt opt(sliceCriteria, sliceInst, sliceFunc, sliceInstNo,
                           true, true);
  vector<Instruction *> startInstrs;
  if (!parseSlicingCriteriaOpt(opt, *M, startInstrs)) {
    errs() << "Please supply the correct slicing criteria (see --help)\n";
    return 1;
  }
  slice(M.get(), startInstrs);
}
