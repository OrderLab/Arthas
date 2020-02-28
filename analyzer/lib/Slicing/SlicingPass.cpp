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

static cl::opt<string> SliceFunc("slice-func",
    cl::desc("Function where the slicing starts"),
    cl::value_desc("function"));

static cl::opt<string> SliceInstr("slice-instr",
                                  cl::desc("Instruction to start slicing"),
                                  cl::value_desc("instruction"));

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
  // Map with persistent variables as key and definition point (metadata) as
  // values
  std::map<Value *, Instruction *> pmemMetadata;
  static char ID;
  SlicingPass() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M) override;
  bool parseSlicingOption(Module &M);

  bool instructionSlice(Instruction *fault_instruction, Function &F, vector<Instruction *> &pmem_instrs, Module &M);
  bool definitionPoint(Function &F, pmem::PMemVariableLocator &locator);
  void pmemInstructionSet(Function &F, pmem::PMemVariableLocator &locator,
                          vector<Instruction *> &pmem_instrs);

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
    if (!FileLine::fromCriteriaStr(SliceCriteria, fileLines)) {
      errs() << "Failed to parse slicing criteria " << SliceCriteria << "\n";
      return false;
    }
    Matcher matcher;
    matcher.process(M);
    vector<MatchResult> matchResults;
    if (!fileLines.empty()) {
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
        errs() << result<< "\n";
      }
    }
  }
  if (!SliceFunc.empty()) {
    if (SliceInstr.empty()) {
      errs() << "Must specify slice instruction when giving a slice function\n";
      return false;
    }
    Function *F;
    bool found = false;
    for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
      F = &*I;
      if (F->getName().equals(SliceFunc)) {
        found = true;
        break;
      }
    }
    if (!found) {
      errs() << "Failed to find slice function " << SliceFunc << "\n";
      return false;
    }
    found = false;
    for (inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ++ii) {
      Instruction * instr = &*ii;
      std::string str_instr;
      llvm::raw_string_ostream rso(str_instr);
      instr->print(rso);
      trim(str_instr);
      if (SliceInstr.compare(str_instr) == 0) {
        errs() << "Found slice instruction " << str_instr << "\n";
        startSliceInstrs.push_back(instr);
        found = true;
        break;
      }
    }
    if (!found) {
      errs() << "Failed to find slice instruction " << SliceInstr << "\n";
      return false;
    }
  }
  sliceDir = SliceDir;
  return true;
}

bool SlicingPass::instructionSlice(Instruction *fault_instruction, Function &F,
                                   vector<Instruction *> &pmem_instrs, Module &M)
{
  ofstream outputFile("program3data.txt");
  outputFile << "starting instructionSlice\n" << flush;;
  llvm::instrument::PmemAddrInstrumenter p_inst;
  p_inst.initHookFuncs(M);

  //Take faulty instruction and walk through Dependency Graph to obtain slices + metadata
  //of persistent variables
  dg::LLVMDependenceGraph *subdg = dgSlicer->getDependenceGraph(&F);
  //dg::LLVMPointerAnalysis *pta = subdg->getPTA();
  outputFile << "got dependence graph for function\n" << flush;;

  //Testing purposes: Using existing slicer first..
  list<list<const Instruction *>> slice_list;
  dg::LLVMSlicer slicer;
  dg::LLVMNode *node = subdg->findNode(fault_instruction);
  SliceGraph sg;

  if(node != nullptr)
    slicer.slice(subdg, &sg, node, 0, 0);
  outputFile << "slicer.slice!!!\n" << flush;;

  errs() << sg.root->total_size(sg.root);
  DgSlices slices;
  sg.root->compute_slices(slices, fault_instruction, SliceDirection::Backward, SlicePersistence::Mixed, 0);
  int count = 0;
  for(auto i = slices.begin(); i != slices.end(); ++i){
    errs() << "slice " << count << " is \n";
    i->dump();
    i->set_persistence(pmem_instrs);
    count++;
    if(count == 3){
      DgSlice i_slice = *i;
      if(i_slice.persistence == SlicePersistence::Volatile){
        errs() << "slice is volatile, do nothing\n";
      }
      else{
        errs() << "slice is persistent or mixed, instrument it\n";
        p_inst.instrumentSlice(i_slice, pmemMetadata);
      }
    }
  }
  sg.root->dump(0);
  outputFile << "Finished slicing\n" << flush;;

  // errs() << "total size of graph is " << sg.root->total_size(sg.root) <<
  // "\n";
  dg::analysis::SlicerStatistics& st = slicer.getStatistics();
  errs() << "INFO: Sliced away " << st.nodesRemoved << " from " << st.nodesTotal << " nodes\n";
  errs() << *fault_instruction << "\n";
  errs() << "Function is " << F << "\n";

  DgSlice *dgSlice;
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
  if (!parseSlicingOption(M)) {
    return false;
  }
  //errs() << "beginning\n";
  dgSlicer = make_unique<DgSlicer>(&M);
  dgSlicer->compute();  // compute the dependence graph for module M

  vector<Instruction *> pmem_instrs;
  //Step 1: PMEM Variable Output Mapping
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    if (F.isDeclaration()) continue;
    pmem::PMemVariableLocator locator(F);
    //for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
    //  errs() << "* Identified pmem variable instruction: " << **vi << "\n";
    //}
    errs() << "begin def point\n";
    definitionPoint(F, locator);
    pmemInstructionSet(F, locator, pmem_instrs);
  }

  errs() << "begin instruction slice \n";;
  //Step 2: Getting Slice of Fault Instruction
  // Hard coded in-input for Fault Instruction

  int a = 0;
  int b = 0;
  Instruction * fault_inst;
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    for (inst_iterator ii = inst_begin(&F), ie = inst_end(&F); ii != ie; ++ii) {
        /*fault_inst = &*ii;
	llvm::errs() << *fault_inst << "\n";
        if(StoreInst *store_inst = dyn_cast<StoreInst>(fault_inst)){
          Value* po = store_inst->getPointerOperand();
          llvm::errs() << "store " << *po << "address " <<  po << "\n";
        }*/
      if(a == 30 && b == 0){
        fault_inst = &*ii;
        instructionSlice(fault_inst, F, pmem_instrs,  M);
        //llvm::errs() << "function is " << F << "\n";
        goto stop;
      }
      a++;
    }
    b++;
  }
  stop:
  llvm::errs() << "done with run on module\n";
  return true;
}

void SlicingPass::pmemInstructionSet(Function &F,
                                     pmem::PMemVariableLocator &locator,
                                     vector<Instruction *> &pmem_list) {
  for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
    Value& b = const_cast<Value&>(**vi); 
    if(Instruction *Inst = dyn_cast<Instruction>(&b))
      pmem_list.push_back(Inst);
  }
}

bool SlicingPass::definitionPoint(Function &F, pmem::PMemVariableLocator &locator) {
  for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
    Value& b = const_cast<Value&>(**vi); 
    UserGraph g(&b);
    for (auto ui = g.begin(); ui != g.end(); ++ui) {
      Value& c = const_cast<Value&>(*ui->first);
      if(Instruction *Inst = dyn_cast<Instruction>(&c)){
        llvm::errs() << "Def point value is " << *Inst << "\n";
        pmemMetadata.insert(std::pair<Value *, Instruction *>(&b, Inst));
      }
    }
  }
  errs() << "size is " << pmemMetadata.size() << "\n";
  errs() << "Finished with definition points for this function \n";

  /*for(auto it = pmemMetadata.begin(); it != pmemMetadata.end(); ++it)
    {
    errs() << *it->first  << " " << *it->second << "\n";
    }*/
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
