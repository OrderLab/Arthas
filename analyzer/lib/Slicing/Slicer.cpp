// The PMEM-Fault Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <chrono>
#include <ctime>
#include <set>
#include <utility>

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "Slicing/Slicer.h"
#include "DefUse/DefUse.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::defuse;

//static cl::list<string> TargetFunctions("target-functions", 
//    cl::desc("<Function>"), cl::ZeroOrMore);

static void printPSNodeName(dg::PSNode *node) {
  string nm;
  const char *name = nullptr;
  if (node->isNull()) {
    name = "null";
  } else if (node->isUnknownMemory()) {
    name = "unknown";
  } else if (node->isInvalidated() && !node->getUserData<llvm::Value>()) {
    name = "invalidated";
  }
  if (!name) {
    const llvm::Value *val = node->getUserData<llvm::Value>();
    if (val) errs() << *val;
  } else {
    errs() << name;
  }
}

static void dumpPSNode(dg::PSNode *n) {
  errs() << "NODE " << n->getID() << ": ";
  printPSNodeName(n);
  errs() << " (points-to size: " << n->pointsTo.size() << ")\n";

  for (const dg::Pointer& ptr : n->pointsTo) {
    errs() << "    -> ";
    printPSNodeName(ptr.target);
    if (ptr.offset.isUnknown())
      errs() << " + Offset::UNKNOWN";
    else
      errs() << " + " << *ptr.offset;
  }
  errs() << "\n";
}

bool DgSlicer::compute() {
  dg::debug::TimeMeasure tm;
  tm.start();
  dg::analysis::pta::PointerAnalysis *pa;
  dg::LLVMPointerAnalysis pta(module);
  // create a flow-sensitive pointer analysis
  pa = pta.createPTA<dg::analysis::pta::PointerAnalysisFS>();
  pa->run();
  tm.stop();
  tm.report("INFO: points-to analysis took");
  const auto &nodes = pta.getNodes();

  errs() << "Points-to graph size " << nodes.size() << "\n";

  dg::llvmdg::LLVMDependenceGraphOptions dg_options;
  dg::llvmdg::LLVMPointerAnalysisOptions pta_options;
  // flow-sensitive
  pta_options.analysisType =
      dg::llvmdg::LLVMPointerAnalysisOptions::AnalysisType::fs;
  dg_options.PTAOptions = pta_options;

  builder = make_unique<dg::llvmdg::LLVMDependenceGraphBuilder>(module, dg_options);

  dg = std::move(builder->constructCFGOnly());
  if (!dg) {
    llvm::errs() << "Building the dependence graph failed!\n";
  }
  // compute both data dependencies (def-use) and control dependencies
  dg = builder->computeDependencies(std::move(dg));

  const auto &stats = builder->getStatistics();
  errs() << "[slicer] CPU time of pointer analysis: "
         << double(stats.ptaTime) / CLOCKS_PER_SEC << " s\n";
  errs() << "[slicer] CPU time of reaching definitions analysis: "
         << double(stats.rdaTime) / CLOCKS_PER_SEC << " s\n";
  errs() << "[slicer] CPU time of control dependence analysis: "
         << double(stats.cdTime) / CLOCKS_PER_SEC << " s\n";
  funcDgMap = &dg::getConstructedFunctions();
  return true;
}

dg::LLVMDependenceGraph *DgSlicer::getDependenceGraph(Function *func)
{
  if (funcDgMap == nullptr)
    return nullptr;
  auto dgit = funcDgMap->find(func);
  if (dgit == funcDgMap->end()) {
    errs() << "Could not find dependency graph for function " << func->getName() << "\n";
  }
  return dgit->second;
}

namespace {
class SlicingPass : public ModulePass {
 public:
  //Map with persistent variables as key and definition point (metadata) as values
  std::map<Value *, Instruction *> pmemMetadata;
  static char ID;
  SlicingPass() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M) override;
  bool instructionSlice(Instruction *fault_instruction, Function &F, std::list<Instruction *>pmem_list);
  bool definitionPoint(Function &F, pmem::PMemVariableLocator locator);
  void pmemInstructionSet(Function &F, pmem::PMemVariableLocator locator,
                                      std::list<Instruction *>& pmem_list);

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

 private:
  bool runOnFunction(Function &F);
  DgSlicer* dgSlicer;
  //Map with Sliced Dependnence Graph as key and persistent variables as values
  static const std::map<dg::LLVMDependenceGraph * , std::set<Value *>> pmemVariablesForSlices;
};
}

bool SlicingPass::instructionSlice(Instruction *fault_instruction, Function &F,
std::list<Instruction *>pmem_list){
  //Take faulty instruction and walk through Dependency Graph to obtain slices + metadata
  //of persistent variables
  dg::LLVMDependenceGraph *subdg = dgSlicer->getDependenceGraph(&F);
  dg::LLVMPointerAnalysis *pta = subdg->getPTA();

  //Testing purposes: Using existing slicer first..
  list<list<const Instruction *>> slice_list;
  dg::LLVMSlicer slicer;
  dg::LLVMNode *node = subdg->findNode(fault_instruction);
  slicegraph::SliceGraph sg;

  if(node != nullptr)
    slicer.slice(subdg, &sg, node, 0, 0);

  errs() << sg.root->total_size(sg.root);
  list<slicegraph::DGSlice> slices;
  sg.root->compute_slices(slices, fault_instruction, slicegraph::SliceDirection::Backward
  , slicegraph::SlicePersistence::Mixed, 0);
  int count = 0;
  for(list<slicegraph::DGSlice>::iterator i = slices.begin(); i != slices.end(); ++i){
    errs() << "slice " << count << " is \n";
    i->print_dgslice();
    i->get_values(pmem_list);
    count++;
  }
  sg.root->print(0);

  //errs() << "total size of graph is " << sg.root->total_size(sg.root) << "\n";
  dg::analysis::SlicerStatistics& st = slicer.getStatistics();
  errs() << "INFO: Sliced away " << st.nodesRemoved << " from " << st.nodesTotal << " nodes\n";

  DgSlice *dgSlice;
  dgSlice->direction = SlicingDirection::Backward;
  dgSlice->fault_instruction = fault_instruction;
  //TODO: persistent state
  dgSlice->slice_id = 0;

  dgSlicer->slices.insert(dgSlice);

  //Forward Slice
  list<list<const Instruction *>>  slice_list2;
  dg::LLVMDependenceGraph *subdg2 = dgSlicer->getDependenceGraph(&F);
  dg::LLVMSlicer slicer2;
  dg::LLVMNode *node2 = subdg2->findNode(fault_instruction);
  slicegraph::SliceGraph sg2;
  if(node2 != nullptr)
    slicer2.slice(subdg2, &sg2, node2, 1, 1);

  dg::analysis::SlicerStatistics& st2 = slicer2.getStatistics();
  errs() << "INFO: Sliced away " << st2.nodesRemoved << " from " << st2.nodesTotal << " nodes\n";

  return false;
}

bool SlicingPass::runOnModule(Module &M) {
  dgSlicer = new DgSlicer(&M);
  dgSlicer->compute();  // compute the dependence graph for module M

  /*bool modified = false;
  set<string> targetFunctionSet(TargetFunctions.begin(), TargetFunctions.end());
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
      if (targetFunctionSet.empty() ||
          targetFunctionSet.count(F.getName()) != 0)
        modified |= runOnFunction(F);
    }
  }
  return modified;*/
  list<Instruction *>pmem_list;
  //Step 1: PMEM Variable Output Mapping
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    pmem::PMemVariableLocator locator(F);
    //for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
    //  errs() << "* Identified pmem variable instruction: " << **vi << "\n";
    //}
    definitionPoint(F, locator);
    pmemInstructionSet(F, locator, pmem_list);
  }

  //Step 2: Getting Slice of Fault Instruction
  // Hard coded in-input for Fault Instruction
  int a = 0;
  Instruction * fault_inst;
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    for (inst_iterator ii = inst_begin(&F), ie = inst_end(&F); ii != ie; ++ii) {
      if(a == 4){
        fault_inst = &*ii;
        instructionSlice(fault_inst, F, pmem_list);
        goto stop;
      }
      a++;
    }
  }
  stop:
  return false;
}

void SlicingPass::pmemInstructionSet(Function &F, pmem::PMemVariableLocator locator,
                                      std::list<Instruction *>& pmem_list){
    for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
      Value& b = const_cast<Value&>(**vi); 
        if(Instruction *Inst = dyn_cast<Instruction>(&b))
          pmem_list.push_back(Inst);
    }


}

bool SlicingPass::definitionPoint(Function &F, pmem::PMemVariableLocator locator){

    for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
      Value& b = const_cast<Value&>(**vi); 
      UserGraph g(&b);
      for (auto ui = g.begin(); ui != g.end(); ++ui) {
        Value& c = const_cast<Value&>(*ui->first);
        if(Instruction *Inst = dyn_cast<Instruction>(&c))
          pmemMetadata.insert(std::pair<Value *, Instruction *>(&b, Inst));
          //pmemMetadata.insert(std::pair<Value *, Instruction *>(&b, &*(ui->first)));
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
