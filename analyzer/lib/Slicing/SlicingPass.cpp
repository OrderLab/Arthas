// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <fstream>
#include <iostream>

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "DefUse/DefUse.h"
#include "Instrument/PmemAddrInstrumenter.h"
#include "Matcher/Matcher.h"
#include "Slicing/Slicer.h"
#include "Utils/String.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::defuse;
using namespace llvm::matching;

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

static cl::opt<string> SliceCriteria(
    "slice-crit", cl::desc("Comma separated list of slicing criterion"),
    cl::value_desc("file1:line1,file2:line2,..."));

namespace {
class SlicingPass : public ModulePass {
 public:
  static char ID;
  SlicingPass() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M) override;
  bool parseSlicingOption(Module &M);

  bool instructionSlice(Instruction *fault_instruction, Function *F,
    pmem::PMemVariableLocator &locator);

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

 private:
  SmallVector<Instruction *, 8> startSliceInstrs;
  SliceDirection sliceDir;

  bool runOnFunction(Function &F);
  unique_ptr<DgSlicer> dgSlicer;
  // Map with Sliced Dependnence Graph as key and persistent variables as values
  static const std::map<dg::LLVMDependenceGraph * , std::set<Value *>> pmemVariablesForSlices;
};
}

bool SlicingPass::parseSlicingOption(Module &M) {
  if (!SliceCriteria.empty()) {
    vector<FileLine> fileLines;
    if (!FileLine::fromCriteriaStr(SliceCriteria, fileLines) || 
        fileLines.empty()) {
      errs() << "Failed to parse slicing criteria " << SliceCriteria << "\n";
      return false;
    }
    Matcher matcher;
    matcher.process(M);
    vector<MatchResult> matchResults;
    if (!matcher.matchInstrsCriteria(fileLines, matchResults)) {
      errs() << "Failed to find the slicing target instructions in module ";
      errs() << M.getName() << " from criteria " << SliceCriteria  << "\n";
      return false;
    }
    errs() << "Found slicing target instructions:\n";
    for (MatchResult &result : matchResults) {
      for (Instruction * instr : result.instrs ) {
        startSliceInstrs.push_back(instr);
      }
      errs() << result << "\n";
    }
    return true;
  }
  if (SliceFunc.empty()) {
    errs() << "Warning: no slicing criteria supplied through command line.\n";
    errs() << "Please supply the slicing criteria (see --help)\n";
    return false;
  }
  if (SliceInst.empty() || SliceInstNo <= 0) {
    errs() << "Must specify slice instruction when giving a slice function\n";
    return false;
  }
  Function *F;
  bool found = false;
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    F = &*I;
    if (!F->isDeclaration() && F->getName().equals(SliceFunc)) {
      found = true;
      break;
    }
  }
  if (!found) {
    errs() << "Failed to find slice function " << SliceFunc << "\n";
    return false;
  }
  found = false;
  int inst_no = 0;
  for (inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ++ii) {
    inst_no++; // instruction no. from 1 to N within the function F
    Instruction * instr = &*ii;
    // if SliceInstNo is specified, we match the instruction simply based
    // on the instruction no.
    if (SliceInstNo > 0) {
      if (SliceInstNo == inst_no) {
        errs() << "Found slice instruction " << instr << "\n";
        startSliceInstrs.push_back(instr);
        found = true;
        break;
      }
    } else { // else we match by the instruction string representation
      std::string str_instr;
      llvm::raw_string_ostream rso(str_instr);
      instr->print(rso);
      trim(str_instr);
      if (SliceInst.compare(str_instr) == 0) {
        errs() << "Found slice instruction " << str_instr << "\n";
        startSliceInstrs.push_back(instr);
        found = true;
        break;
      }
    }
  }
  if (!found) {
    errs() << "Failed to find slice instruction ";
    if (SliceInstNo > 0) {
      errs() << "no. " << SliceInstNo;
    } else {
      errs() << SliceInst;
    }
    errs() << " in function " << SliceFunc << "\n";
    return false;
  }
  sliceDir = SliceDir;
  return true;
}

bool SlicingPass::instructionSlice(Instruction *fault_instruction, Function *F,
            pmem::PMemVariableLocator &locator) 
{
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
  DgSlice *dgSlice = new DgSlice();
  dgSlice->direction = SliceDirection::Backward;
  dgSlice->root_instr = fault_instruction;
  // TODO: persistent state
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
  if (!parseSlicingOption(M)) {
    return false;
  }
  // Step 2: Compute dependence graph and slicer for the module
  dgSlicer = make_unique<DgSlicer>(&M);
  dgSlicer->compute();  // compute the dependence graph for module M

  for (auto fault_inst : startSliceInstrs) {
    Function *F = fault_inst->getFunction();
    // Step 3: PMEM variable location and mapping
    pmem::PMemVariableLocator locator;
    locator.runOnFunction(*F);
    locator.findDefinitionPoints();
    instructionSlice(fault_inst, F, locator);
  }
  llvm::errs() << "Done with run on module\n";
  return true;
}

bool SlicingPass::runOnFunction(Function &F) {
  errs() << "[" << F.getName() << "]\n";
  dg::LLVMDependenceGraph *subdg = dgSlicer->getDependenceGraph(&F);
  dg::LLVMPointerAnalysis *pta = subdg->getPTA();

  //Testing purposes: Using existing slicer first..
  dg::LLVMSlicer slicer;

  if (subdg != nullptr) {
    for (inst_iterator ii = inst_begin(&F), ie = inst_end(&F); ii != ie; ++ii) {
      Instruction *inst = &*ii;
      inst->dump();
      if (pta != nullptr) {
        dg::LLVMPointsToSet pts = pta->getLLVMPointsTo(inst);
        errs() << "// points-to-set (size " << pts.size() << "):\n";
        for (auto ptri = pts.begin(); ptri != pts.end(); ++ptri) {
          dg::LLVMPointer ptr = *ptri;
          errs() << "//   o->" << *ptr.value << "\n";
        }
      }
      dg::LLVMNode *node = subdg->findNode(inst);
      //if(node != nullptr)
      //  slicer.slice(subdg, node, 0, 0);
      if (node != nullptr && node->getDataDependenciesNum() > 0) {
        errs() << "// " << node->getDataDependenciesNum() << " data dependency:\n";
        for (auto di = node->data_begin(); di != node->data_end(); ++di) {
          Value *dep = (*di)->getValue();
          errs() << "//   =>" << *dep << "\n";
        }
      } else {
        errs() << "// no data dependency found\n";
      }
    }
  }
  dg::analysis::SlicerStatistics& st = slicer.getStatistics();
  errs() << "INFO: Sliced away " << st.nodesRemoved << " from " << st.nodesTotal << " nodes\n";
  return false;
}

char SlicingPass::ID = 0;
static RegisterPass<SlicingPass> X("slicer", "Slices the code");
