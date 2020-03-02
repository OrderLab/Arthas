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

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
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

  virtual bool runOnModule(Module &M) override;

  bool instructionSlice(Instruction *fault_instruction, Function *F,
    pmem::PMemVariableLocator &locator);

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

 private:
  vector<Instruction *> startSliceInstrs;
  SliceDirection sliceDir;

  unique_ptr<DgSlicer> dgSlicer;
  // Map with Sliced Dependnence Graph as key and persistent variables as values
  static const std::map<dg::LLVMDependenceGraph * , std::set<Value *>> pmemVariablesForSlices;
};
}

bool SlicingPass::instructionSlice(Instruction *fault_instruction, Function *F,
                                   pmem::PMemVariableLocator &locator) {
  DEBUG(dbgs() << "starting instruction slice\n");

  //Take faulty instruction and walk through Dependency Graph to obtain slices + metadata
  //of persistent variables
  dg::LLVMDependenceGraph *subdg = dgSlicer->getDependenceGraph(F);
  // dg::LLVMPointerAnalysis *pta = subdg->getPTA();
  DEBUG(dbgs() << "got dependence graph for function\n");

  //Testing purposes: Using existing slicer first..
  list<list<const Instruction *>> slice_list;
  dg::LLVMSlicer slicer;
  dg::LLVMNode *node = subdg->findNode(fault_instruction);
  SliceGraph sg;

  if (node != nullptr)
    slicer.slice(subdg, &sg, node, 0, 0);
  DEBUG(dbgs() << "slicer.slice!!!\n");

  errs() << sg.root->total_size(sg.root);
  DgSlices slices;
  sg.root->compute_slices(slices, fault_instruction, SliceDirection::Backward, SlicePersistence::Mixed, 0);
  int count = 0;
  for (auto i = slices.begin(); i != slices.end(); ++i) {
    DgSlice *slice = *i;
    errs() << "slice " << count << " is \n";
    slice->dump();
    slice->set_persistence(locator.vars());
    count++;
    if (count == 3) {
      if (slice->persistence == SlicePersistence::Volatile){
        errs() << "slice is volatile, do nothing\n";
      }
      else {
        errs() << "slice is persistent or mixed, instrument it\n";
        // TODO: if we instrument pmem addr in the first-run, then we don't need 
        // to instrument the program anymore in the slicing stage. Otherwise, 
        // we will instrument the persistent points in the slice.
      }
    }
  }
  sg.root->dump(0);
  DEBUG(dbgs() << "Finished slicing\n");

  // errs() << "total size of graph is " << sg.root->total_size(sg.root) <<
  // "\n";
  dg::analysis::SlicerStatistics& st = slicer.getStatistics();
  errs() << "INFO: Sliced away " << st.nodesRemoved << " from " << st.nodesTotal << " nodes\n";
  errs() << *fault_instruction << "\n";
  errs() << "Function is " << F << "\n";

  // FIXME: wtf???
  //This is a remnant from old code, no longer being used
  DgSlice *dgSlice = new DgSlice();
  dgSlice->direction = SliceDirection::Backward;
  dgSlice->root_instr = fault_instruction;
  dgSlice->slice_id = 0;

  dgSlicer->slices.insert(dgSlice);

  //Forward Slice
  /*list<list<const Instruction *>>  slice_list2;
  dg::LLVMDependenceGraph *subdg2 = dgSlicer->getDependenceGraph(&F);
  dg::LLVMSlicer slicer2;
  dg::LLVMNode *node2 = subdg2->findNode(fault_instruction);
  errs() << "forward slice about\n";
  SliceGraph sg2;
  if (node2 != nullptr) slicer2.slice(subdg2, &sg2, node2, 1, 1);

  errs() << "forward sliced\n";
  dg::analysis::SlicerStatistics& st2 = slicer2.getStatistics();
  errs() << "INFO: Sliced away " << st2.nodesRemoved << " from " << st2.nodesTotal << " nodes\n";
  */
  return true;
}

bool SlicingPass::runOnModule(Module &M) {
  errs() << "Begin instruction slice \n";
  // Step 1: Getting the initial fault instruction
  //  Fault instruction is specified through the slicing criteria in command line.
  //  See the opt declaration at the beginning of this file.
  SliceInstCriteriaOpt opt(SliceFileLines, SliceInst, SliceFunc, SliceInstNo);
  if (!parseSlicingCriteriaOpt(opt, M, startSliceInstrs)) {
    errs() << "Please supply the correct slicing criteria (see --help)\n";
    return false;
  }
  sliceDir = SliceDir;

  // Step 2: Compute dependence graph and slicer for the module
  dgSlicer = make_unique<DgSlicer>(&M);
  dgSlicer->compute();  // compute the dependence graph for module M

  for (auto fault_inst : startSliceInstrs) {
    Function *F = fault_inst->getFunction();
    // Step 3: PMEM variable location and mapping
    pmem::PMemVariableLocator locator;
    locator.runOnFunction(*F);
    locator.findDefinitionPoints();
    errs() << "Fault instruction " << fault_inst;
    // instructionSlice(fault_inst, F, locator);
  }
  llvm::errs() << "Done with run on module\n";
  return true;
}

char SlicingPass::ID = 0;
static RegisterPass<SlicingPass> X("slicer", "Slices the code");
