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

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "DefUse/DefUse.h"
#include "Instrument/PmemAddrInstrumenter.h"
#include "Slicing/SliceCriteria.h"
#include "Slicing/Slicer.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::defuse;

#define DEBUG_TYPE "slicing-pass"

static cl::opt<string> SliceOutput(
    "slice-output", cl::desc("File to write the slice output to"),
    cl::init("slices.log"), cl::value_desc("file"));

static cl::opt<string> SliceFunc("slice-func",
    cl::desc("Function where the slicing starts"),
    cl::value_desc("function"));

static cl::opt<string> SliceInst("slice-inst",
                                  cl::desc("Instruction to start slicing"),
                                  cl::value_desc("instruction"));

static cl::opt<int> SliceInstNo(
    "slice-inst-no", cl::desc("Nth instruction in a function to start slicing"),
    cl::init(0), cl::value_desc("instruction"));

static cl::opt<SliceDirection> SliceDir(
    "slice-dir", cl::desc("Slicing direction"),
    cl::values(
        clEnumValN(SliceDirection::Backward, "backward", "Backward slicing"),
        clEnumValN(SliceDirection::Forward, "forward", "Forward slicing"),
        clEnumValN(SliceDirection::Full, "full", "Full slicing"),
        clEnumValEnd));

static cl::opt<string> SliceFileLines(
    "slice-crit", cl::desc("Comma separated list of slicing criterion"),
    cl::value_desc("file1:line1,file2:line2,..."));

namespace {
class SlicingPass : public ModulePass {
 public:
  static char ID;
  SlicingPass() : ModulePass(ID) {}

  ~SlicingPass();

  virtual bool runOnModule(Module &M) override;

  bool instructionSlice(Instruction *fault_inst, Function *F,
                        PMemVariableLocator *locator);

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

 private:
  vector<Instruction *> _startSliceInstrs;
  SliceDirection _sliceDir;
  unique_ptr<DgSlicer> _dgSlicer;
  raw_ostream *_out_stream;
  bool _delete_out_stream;

  // Map with Sliced Dependnence Graph as key and persistent variables as values
  static const std::map<dg::LLVMDependenceGraph * , std::set<Value *>> pmemVariablesForSlices;
};
}

SlicingPass::~SlicingPass()
{
  if (_out_stream && _delete_out_stream) {
    // only delete the out stream if it's being new-ed (i.e., file out stream)
    delete _out_stream;
    _delete_out_stream = false;
  }
}

bool SlicingPass::instructionSlice(Instruction *fault_inst, Function *F,
                                   PMemVariableLocator *locator) {
  // Take faulty instruction and walk through Dependency Graph to 
  // obtain slices + metadata of persistent variables
  dg::LLVMDependenceGraph *subdg = _dgSlicer->getDependenceGraph(F);
  if (subdg == nullptr) {
    errs() << "Failed to find dependence graph for " << F->getName() << "\n";
    return false;
  }
  errs() << "Got dependence graph for function " << F->getName() << "\n";
  dg::LLVMNode *node = subdg->findNode(fault_inst);
  if (node == nullptr) {
    errs() << "Failed to find LLVMNode for " << *fault_inst << ", cannot slice\n";
    return false;
  }
  errs() << "Computing slice for fault instruction " << *fault_inst << "\n";
  SliceGraph sg(new SliceNode(fault_inst, 0));
  _dgSlicer->slice(node, &sg);

  auto& st = _dgSlicer->getStatistics();
  errs() << "INFO: Sliced away " << st.nodesRemoved << " from " << st.nodesTotal
         << " nodes\n";

  return true;

  Slices slices;

  for (auto i = slices.begin(); i != slices.end(); ++i) {
    Slice *slice = *i;
    slice->setPersistence(locator->vars());
    slice->dump(*_out_stream);
    if (slice->persistence == SlicePersistence::Volatile) {
      errs() << "Slice is volatile, do nothing\n";
    } else {
      errs() << "Slice is persistent or mixed, instrument it\n";
      // TODO: if we instrument pmem addr in the first-run, then we don't need 
      // to instrument the program anymore in the slicing stage. Otherwise, 
      // we will instrument the persistent points in the slice.
    }
  }
  errs() << "Finished slicing. Total size of slice graph: ";
  errs() << sg.root->total_size(sg.root) << "\n";
  errs() << "Slices are written to " << SliceOutput << "\n";
  return true;
}

bool SlicingPass::runOnModule(Module &M) {
  if (!SliceOutput.empty()) {
    std::error_code ec;
    _out_stream = new raw_fd_ostream(SliceOutput, ec, sys::fs::F_Text);
    _delete_out_stream = true;
  } else {
    _out_stream = &errs();
  }

  errs() << "Begin instruction slice \n";
  // Step 1: Getting the initial fault instruction
  //  Fault instruction is specified through the slicing criteria in command line.
  //  See the opt declaration at the beginning of this file.
  SliceInstCriteriaOpt opt(SliceFileLines, SliceInst, SliceFunc, SliceInstNo);
  if (!parseSlicingCriteriaOpt(opt, M, _startSliceInstrs)) {
    errs() << "Please supply the correct slicing criteria (see --help)\n";
    return false;
  }
  _sliceDir = SliceDir;

  // Step 2: Compute dependence graph and slicer for the module
  _dgSlicer = make_unique<DgSlicer>(&M, SliceDirection::Backward);
  _dgSlicer->computeDependencies();  // compute the dependence graph for module M

  // Step 3: PMEM variable location and mapping
  map<Function *, unique_ptr<PMemVariableLocator>> locatorMap;
  for (auto fault_inst : _startSliceInstrs) {
    Function *F = fault_inst->getFunction();
    auto li = locatorMap.find(F);
    if (li == locatorMap.end()) {
      locatorMap.insert(make_pair(F, make_unique<PMemVariableLocator>()));
    }
    PMemVariableLocator *locator = locatorMap[F].get();
    locator->runOnFunction(*F);
    locator->findDefinitionPoints();
    instructionSlice(fault_inst, F, locator);
  }
  llvm::errs() << "Done with run on module\n";
  return false;
}

char SlicingPass::ID = 0;
static RegisterPass<SlicingPass> X("slicer", "Slices the code");
