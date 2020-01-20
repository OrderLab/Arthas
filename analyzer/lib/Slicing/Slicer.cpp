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

using namespace std;
using namespace llvm;
using namespace llvm::slicing;

static cl::list<string> TargetFunctions("target-functions", 
    cl::desc("<Function>"), cl::ZeroOrMore);

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
  static char ID;
  SlicingPass() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M) override;
  bool instructionSlice(Instruction *fault_instruction, dg::LLVMDependenceGraph *subdg,
  dg::LLVMPointerAnalysis *pta);

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

 private:
  bool runOnFunction(Function &F);
  DgSlicer* dgSlicer;
  //Map with Sliced Dependnence Graph as key and persistent variables as values
  static const std::map<dg::LLVMDependenceGraph * , std::set<Value *>> pmemVariablesForSlices;
  //Map with persistent variables as key and definition point (metadata) as values
  static const std::map<Value *, Instruction *> pmemMetadata;
};
}

bool SlicingPass::instructionSlice(Instruction *fault_instruction, dg::LLVMDependenceGraph *subdg,
dg::LLVMPointerAnalysis *pta){
  //TODO: Implement after finishing workflow
  //Take faulty instruction and walk through Dependency Graph to obtain slices + metadata
  //of persistent variables
  return false;
}

bool SlicingPass::runOnModule(Module &M) {
  dgSlicer = new DgSlicer(&M);
  dgSlicer->compute();  // compute the dependence graph for module M

  bool modified = false;
  set<string> targetFunctionSet(TargetFunctions.begin(), TargetFunctions.end());
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration()) {
      if (targetFunctionSet.empty() ||
          targetFunctionSet.count(F.getName()) != 0)
        modified |= runOnFunction(F);
    }
  }
  return modified;
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
      if(node != nullptr)
        slicer.slice(subdg, node, 0);
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
