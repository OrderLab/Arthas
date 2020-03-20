#ifndef HAVE_LLVM
#error "Need LLVM for LLVMDependenceGraph"
#endif

#ifndef ENABLE_CFG
#error "Need CFG enabled for building LLVM Dependence Graph"
#endif

#include <set>
#include <unordered_map>
#include <utility>

// ignore unused parameters in LLVM libraries
#if (__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <llvm/Config/llvm-config.h>

#if ((LLVM_VERSION_MAJOR == 3) && (LLVM_VERSION_MINOR < 5))
#include <llvm/Support/CFG.h>
#else
#include <llvm/IR/CFG.h>
#endif

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

#if (__clang__)
#pragma clang diagnostic pop  // ignore -Wunused-parameter
#else
#pragma GCC diagnostic pop
#endif

#include "../lib/llvm/analysis/ControlDependence/NonTerminationSensitiveControlDependencyAnalysis.h"
#include "dg/llvm/LLVMDependenceGraph.h"
#include "dg/llvm/LLVMNode.h"
#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"

#include "llvm-utils.h"
#include "llvm/LLVMDGVerifier.h"
#include "llvm/analysis/ControlExpression.h"

#include "dg/ADT/Queue.h"

#include "analysis/DefUse/DefUse.h"

#include "dg/llvm/analysis/ThreadRegions/ControlFlowGraph.h"
#include "dg/llvm/analysis/ThreadRegions/MayHappenInParallel.h"

using llvm::errs;
using std::make_pair;

namespace dg {

/// ------------------------------------------------------------------
//  -- LLVMDependenceGraph
/// ------------------------------------------------------------------

// map of all constructed functions
std::map<llvm::Value *, LLVMDependenceGraph *> constructedFunctions;

const std::map<llvm::Value *, LLVMDependenceGraph *>
    &getConstructedFunctions() {
  return constructedFunctions;
}

LLVMDependenceGraph::~LLVMDependenceGraph() {
  // delete nodes
  for (auto I = begin(), E = end(); I != E; ++I) {
    LLVMNode *node = I->second;

    if (node) {
      for (LLVMDependenceGraph *subgraph : node->getSubgraphs()) {
        // graphs are referenced, once the refcount is 0
        // the graph will be deleted
        // Because of recursive calls, graph can be its
        // own subgraph. In that case we're in the destructor
        // already, so do not delete it
        subgraph->unref(subgraph != this);
      }

      // delete parameters (on null it is no op)
      delete node->getParameters();

#ifdef DEBUG_ENABLED
      if (!node->getBBlock() && !llvm::isa<llvm::Function>(*I->first))
        llvmutils::printerr("Had no BB assigned", I->first);
#endif

      delete node;
    } else {
#ifdef DEBUG_ENABLED
      llvmutils::printerr("Had no node assigned", I->first);
#endif  //
    }
  }

  // delete global nodes if this is the last graph holding them
  if (global_nodes && global_nodes.use_count() == 1) {
    for (auto &it : *global_nodes) delete it.second;
  }

  // delete formal parameters
  delete getParameters();

  // delete post-dominator tree root
  delete getPostDominatorTreeRoot();
}

static void addGlobals(llvm::Module *m, LLVMDependenceGraph *dg) {
  // create a container for globals,
  // it will be inherited to subgraphs
  dg->allocateGlobalNodes();

  for (auto I = m->global_begin(), E = m->global_end(); I != E; ++I)
    dg->addGlobalNode(new LLVMNode(&*I));
}

bool LLVMDependenceGraph::verify() const {
  LLVMDGVerifier verifier(this);
  return verifier.verify();
}

void LLVMDependenceGraph::setThreads(bool threads) { this->threads = threads; }

LLVMNode *LLVMDependenceGraph::findNode(llvm::Value *value) const {
  auto iterator = nodes.find(value);
  if (iterator != nodes.end()) {
    return iterator->second;
  } else {
    return nullptr;
  }
}

bool LLVMDependenceGraph::build(llvm::Module *m, llvm::Function *entry) {
  // get entry function if not given
  if (entry)
    entryFunction = entry;
  else
    entryFunction = m->getFunction("main");

  if (!entryFunction) {
    errs() << "No entry function found/given\n";
    return false;
  }

  module = m;

  // add global nodes. These will be shared across subgraphs
  addGlobals(m, this);

  // build recursively DG from entry point
  build(entryFunction);

  // Arthas Changes:
  //
  //  This is a fail-safe step that constructs dependency graph for
  //  any function that does not have a dg. The reason this could happen
  //  includes (1) we disabled pointer analysis for efficiency; (2)
  //  we enabled intra-procedural analysis and wanted to query dg
  //  for functions other than the entry function; (3) we enabled
  //  pointer analysis but the PA missed some potential elements
  //  in the point-to set for some call instruction (thus the
  //  actual call target does not have a dg)
  auto mainFunc = m->getFunction("main");
  for (llvm::Function &func : *module) {
    if (func.isDeclaration() || func.size() == 0) continue;
    if (constructedFunctions.find(&func) == constructedFunctions.end()) {
      if (mainFunc) {
        errs() << "Trying to create one subgraph for " << func.getName()
               << "() that is connected to main()\n";
        // here we first create a fake call instruction to the
        // functions without a constructed dg
        llvm::CallInst *ci = llvm::CallInst::Create(&func);
        // then we create a phony LLVMNode
        LLVMNode *phony = new LLVMNode(ci, true);
        // then we add this node to the current dg
        addNode(phony);
        LLVMBBlock *exitBB = getExitBB();
        // then we assign this phony LLVMNode to the exit BB of main
        // we must do so because other places will call getBBlock for a node
        phony->setBasicBlock(exitBB);
        // then we build the subgraph for the func
        LLVMDependenceGraph *subg = buildSubgraph(phony, &func);
        // then we add this subdg to the phony node, without this step
        // we won't be able to iterate through the subdg, e.g., when
        // adding def-use edges
        phony->addSubgraph(subg);
      } else {
        errs() << "No dependency graph constructed for " << func.getName()
               << "()\n";
      }
    }
  }
  return true;
};

LLVMDependenceGraph *LLVMDependenceGraph::buildSubgraph(LLVMNode *node) {
  using namespace llvm;

  Value *val = node->getValue();
  CallInst *CInst = dyn_cast<CallInst>(val);
  assert(CInst && "buildSubgraph called on non-CallInst");
  Function *callFunc = CInst->getCalledFunction();

  return buildSubgraph(node, callFunc);
}

// FIXME don't duplicate the code
bool LLVMDependenceGraph::addFormalGlobal(llvm::Value *val) {
  // add the same formal parameters
  LLVMDGParameters *params = getOrCreateParameters();
  assert(params);

  // if we have this value, just return
  if (params->find(val)) return false;

  LLVMNode *fpin, *fpout;
  std::tie(fpin, fpout) = params->constructGlobal(val, val, this);
  assert(fpin && fpout);
  assert(fpin->getDG() == this && fpout->getDG() == this);

  LLVMNode *entry = getEntry();
  entry->addControlDependence(fpin);
  entry->addControlDependence(fpout);

  // if these are the formal parameters of the main
  // function, add control dependence also between the
  // global as the formal input parameter representing this global
  if (llvm::Function *F = llvm::dyn_cast<llvm::Function>(entry->getValue())) {
    if (F == entryFunction) {
      auto gnode = getGlobalNode(val);
      assert(gnode);
      gnode->addControlDependence(fpin);
    }
  }

  return true;
}

static bool addSubgraphGlobParams(LLVMDependenceGraph *parentdg,
                                  LLVMDGParameters *params) {
  bool changed = false;
  for (auto it = params->global_begin(), et = params->global_end(); it != et;
       ++it)
    changed |= parentdg->addFormalGlobal(it->first);

  // and add heap-allocated variables
  for (const auto &it : *params) {
    if (llvm::isa<llvm::CallInst>(it.first))
      changed |= parentdg->addFormalParameter(it.first);
  }

  return changed;
}

void LLVMDependenceGraph::addSubgraphGlobalParameters(
    LLVMDependenceGraph *subgraph) {
  LLVMDGParameters *params = subgraph->getParameters();
  if (!params) return;

  // if we do not change anything, it means that the graph
  // has these params already, and so must the callers of the graph
  if (!addSubgraphGlobParams(this, params)) return;

  // recursively add the formal parameters to all callers
  for (LLVMNode *callsite : this->getCallers()) {
    LLVMDependenceGraph *graph = callsite->getDG();
    graph->addSubgraphGlobalParameters(this);

    // update the actual parameters of the call-site
    callsite->addActualParameters(this);
  }
}

LLVMDependenceGraph *LLVMDependenceGraph::buildSubgraph(
    LLVMNode *node, llvm::Function *callFunc, bool fork) {
  using namespace llvm;

  LLVMBBlock *BB;

  // if we don't have this subgraph constructed, construct it
  // else just add call edge
  LLVMDependenceGraph *&subgraph = constructedFunctions[callFunc];
  if (!subgraph) {
    // since we have reference the the pointer in
    // constructedFunctions, we can assing to it
    subgraph = new LLVMDependenceGraph();
    // set global nodes to this one, so that
    // we'll share them
    subgraph->setGlobalNodes(getGlobalNodes());
    subgraph->module = module;
    subgraph->PTA = PTA;
    subgraph->RDA = RDA;
    subgraph->threads = this->threads;
    // make subgraphs gather the call-sites too
    subgraph->gatherCallsites(gather_callsites, gatheredCallsites);

// make the real work
#ifndef NDEBUG
    bool ret =
#endif
        subgraph->build(callFunc);

#ifndef NDEBUG
    // at least for now use just assert, if we'll
    // have a reason to handle such failures at some
    // point, we can change it
    assert(ret && "Building subgraph failed");
#endif

    // we built the subgraph, so it has refcount = 1,
    // later in the code we call addSubgraph, which
    // increases the refcount to 2, but we need this
    // subgraph to has refcount 1, so unref it
    subgraph->unref(false /* deleteOnZero */);
  }

  BB = node->getBBlock();
  assert(BB && "do not have BB; this is a bug, sir");
  BB->addCallsite(node);

  // add control dependence from call node
  // to entry node
  node->addControlDependence(subgraph->getEntry());

  // add globals that are used in subgraphs
  // it is necessary if this subgraph was creating due to function
  // pointer call
  addSubgraphGlobalParameters(subgraph);
  node->addActualParameters(subgraph, callFunc, fork);

  if (auto noret = subgraph->getNoReturn()) {
    assert(node->getParameters());  // we created them a while ago
    auto actnoret = getOrCreateNoReturn(node);
    noret->addControlDependence(actnoret);
  }

  return subgraph;
}

static bool is_func_defined(const llvm::Function *func) {
  if (!func || func->size() == 0) return false;

  return true;
}

LLVMNode *LLVMDependenceGraph::getOrCreateNoReturn() {
  // add the same formal parameters
  LLVMDGParameters *params = getOrCreateParameters();
  LLVMNode *noret = params->getNoReturn();
  if (!noret) {
    auto UI = new llvm::UnreachableInst(getModule()->getContext());
    noret = new LLVMNode(UI, true);
    params->addNoReturn(noret);
    auto entry = getEntry();
    assert(entry);
    entry->addControlDependence(noret);
  }
  return noret;
}

LLVMNode *LLVMDependenceGraph::getOrCreateNoReturn(LLVMNode *call) {
  LLVMDGParameters *params = call->getParameters();
  assert(params);

  LLVMNode *noret = params->getNoReturn();
  if (!noret) {
    auto UI = new llvm::UnreachableInst(getModule()->getContext());
    noret = new LLVMNode(UI, true);
    params->addNoReturn(noret);
    // this edge is redundant...
    call->addControlDependence(noret);
  }
  return noret;
}

LLVMDGParameters *LLVMDependenceGraph::getOrCreateParameters() {
  LLVMDGParameters *params = getParameters();
  if (!params) {
    params = new LLVMDGParameters();
    setParameters(params);
  }

  return params;
}

bool LLVMDependenceGraph::addFormalParameter(llvm::Value *val) {
  // add the same formal parameters
  LLVMDGParameters *params = getOrCreateParameters();

  // if we have this value, just return
  if (params->find(val)) return false;

  LLVMNode *fpin, *fpout;
  std::tie(fpin, fpout) = params->construct(val, val, this);
  assert(fpin && fpout);
  assert(fpin->getDG() == this && fpout->getDG() == this);

  LLVMNode *entry = getEntry();
  entry->addControlDependence(fpin);
  entry->addControlDependence(fpout);

  // if these are the formal parameters of the main
  // function and val is a global variable,
  // add control dependence also between the global
  // and the formal input parameter representing this global
  if (llvm::isa<llvm::GlobalVariable>(val)) {
    if (llvm::Function *F = llvm::dyn_cast<llvm::Function>(entry->getValue())) {
      if (F == entryFunction) {
        auto gnode = getGlobalNode(val);
        assert(gnode);
        gnode->addControlDependence(fpin);
      }
    }
  }

  return true;
}

// FIXME copied from PointsTo.cpp, don't duplicate,
// add it to analysis generic
static bool isMemAllocationFunc(const llvm::Function *func) {
  if (!func || !func->hasName()) return false;

  const llvm::StringRef &name = func->getName();
  if (name.equals("malloc") || name.equals("calloc") || name.equals("realloc"))
    return true;

  return false;
}

void LLVMDependenceGraph::handleInstruction(llvm::Value *val, LLVMNode *node,
                                            LLVMNode *prevNode) {
  using namespace llvm;

  if (CallInst *CInst = dyn_cast<CallInst>(val)) {
    Value *strippedValue = CInst->getCalledValue()->stripPointerCasts();
    Function *func = dyn_cast<Function>(strippedValue);
    // if func is nullptr, then this is indirect call
    // via function pointer. If we have the points-to information,
    // create the subgraph
    if (!func && !CInst->isInlineAsm() && PTA) {
      using namespace analysis::pta;
      if (PSNode *op = PTA->getPointsTo(strippedValue)) {
        for (const Pointer &ptr : op->pointsTo) {
          if (!ptr.isValid() || ptr.isInvalidated()) continue;

          // vararg may introduce imprecision here, so we
          // must check that it is really pointer to a function
          if (!isa<Function>(ptr.target->getUserData<Value>())) continue;

          Function *F = ptr.target->getUserData<Function>();

          if (F->size() == 0 || !llvmutils::callIsCompatible(F, CInst)) {
            if (threads && F && F->getName() == "pthread_create") {
              auto possibleFunctions =
                  PTA->getPointsToFunctions(CInst->getArgOperand(2));
              for (auto &function : possibleFunctions) {
                if (function->size() > 0) {
                  LLVMDependenceGraph *subg = buildSubgraph(
                      node, const_cast<llvm::Function *>(function),
                      true /*this is fork*/);
                  node->addSubgraph(subg);
                }
              }
            } else {
              // incompatible prototypes or the function
              // is only declaration
              continue;
            }
          } else {
            LLVMDependenceGraph *subg = buildSubgraph(node, F);
            node->addSubgraph(subg);
          }
        }
      } else
        llvmutils::printerr("Had no PTA node", strippedValue);
    }

    if (func && gather_callsites && func->getName().equals(gather_callsites)) {
      gatheredCallsites->insert(node);
    }

    if (is_func_defined(func)) {
      LLVMDependenceGraph *subg = buildSubgraph(node, func);
      node->addSubgraph(subg);
    }

    // if we allocate a memory in a function, we can pass
    // it to other functions, so it is like global.
    // We need it as parameter, so that if we define it,
    // we can add def-use edges from parent, through the parameter
    // to the definition
    if (isMemAllocationFunc(CInst->getCalledFunction()))
      addFormalParameter(val);

    if (threads && func && func->getName() == "pthread_create") {
      auto possibleFunctions =
          PTA->getPointsToFunctions(CInst->getArgOperand(2));
      for (auto &function : possibleFunctions) {
        LLVMDependenceGraph *subg =
            buildSubgraph(node, const_cast<llvm::Function *>(function),
                          true /*this is fork*/);
        node->addSubgraph(subg);
      }
    }

    // no matter what is the function, this is a CallInst,
    // so create call-graph
    addCallNode(node);
  } else if (isa<UnreachableInst>(val)) {
    auto noret = getOrCreateNoReturn();
    node->addControlDependence(noret);
    // unreachable is being inserted because of the previous instr
    // aborts the program. This means that whether it is executed
    // depends on the previous instr
    if (prevNode) prevNode->addControlDependence(noret);
  } else if (Instruction *Inst = dyn_cast<Instruction>(val)) {
    if (isa<LoadInst>(val) || isa<GetElementPtrInst>(val)) {
      Value *op = Inst->getOperand(0)->stripInBoundsOffsets();
      if (isa<GlobalVariable>(op)) addFormalGlobal(op);
    } else if (isa<StoreInst>(val)) {
      Value *op = Inst->getOperand(0)->stripInBoundsOffsets();
      if (isa<GlobalVariable>(op)) addFormalGlobal(op);

      op = Inst->getOperand(1)->stripInBoundsOffsets();
      if (isa<GlobalVariable>(op)) addFormalGlobal(op);
    }
  }
}

LLVMBBlock *LLVMDependenceGraph::build(llvm::BasicBlock &llvmBB) {
  using namespace llvm;

  LLVMBBlock *BB = new LLVMBBlock();
  LLVMNode *node = nullptr;
  LLVMNode *prevNode = nullptr;

  BB->setKey(&llvmBB);

  // iterate over the instruction and create node for every single one of them
  for (Instruction &Inst : llvmBB) {
    prevNode = node;

    Value *val = &Inst;
    node = new LLVMNode(val);

    // add new node to this dependence graph
    addNode(node);

    // add the node to our basic block
    BB->append(node);

    // take instruction specific actions
    handleInstruction(val, node, prevNode);
  }

  // did we created at least one node?
  if (!node) {
    assert(llvmBB.empty());
    return BB;
  }

  // check if this is the exit node of function
  // (node is now the last instruction in this BB)
  // if it is, connect it to one artificial return node
  Value *termval = node->getValue();
  if (isa<ReturnInst>(termval)) {
    // create one unified exit node from function and add control dependence
    // to it from every return instruction. We could use llvm pass that
    // would do it for us, but then we would lost the advantage of working
    // on dep. graph that is not for whole llvm
    LLVMNode *ext = getExit();
    if (!ext) {
      // we need new llvm value, so that the nodes won't collide
      ReturnInst *phonyRet = ReturnInst::Create(termval->getContext());
      if (!phonyRet) {
        errs() << "ERR: Failed creating phony return value "
               << "for exit node\n";
        // XXX later we could return somehow more mercifully
        abort();
      }

      ext = new LLVMNode(phonyRet, true /* node owns the value -
                                                 it will delete it */);
      setExit(ext);

      LLVMBBlock *retBB = new LLVMBBlock(ext);
      retBB->deleteNodesOnDestruction();
      setExitBB(retBB);
      assert(!unifiedExitBB &&
             "We should not have it assinged yet (or again) here");
      unifiedExitBB = std::unique_ptr<LLVMBBlock>(retBB);
    }

    // add control dependence from this (return) node to EXIT node
    assert(node && "BUG, no node after we went through basic block");
    node->addControlDependence(ext);
    // 255 is maximum value of uint8_t which is the type of the label
    // of the edge
    BB->addSuccessor(getExitBB(), 255);
  }

  // sanity check if we have the first and the last node set
  assert(BB->getFirstNode() && "No first node in BB");
  assert(BB->getLastNode() && "No last node in BB");

  return BB;
}

static LLVMBBlock *createSingleExitBB(LLVMDependenceGraph *graph) {
  llvm::UnreachableInst *ui =
      new llvm::UnreachableInst(graph->getModule()->getContext());
  LLVMNode *exit = new LLVMNode(ui, true);
  graph->addNode(exit);
  graph->setExit(exit);
  LLVMBBlock *exitBB = new LLVMBBlock(exit);
  graph->setExitBB(exitBB);

  // XXX should we add predecessors? If the function does not
  // return anything, we don't need propagate anything outside...
  return exitBB;
}

static void addControlDepsToPHI(LLVMDependenceGraph *graph, LLVMNode *node,
                                const llvm::PHINode *phi) {
  using namespace llvm;

  const BasicBlock *this_block = phi->getParent();
  auto &CB = graph->getBlocks();

  for (auto I = phi->block_begin(), E = phi->block_end(); I != E; ++I) {
    BasicBlock *B = *I;

    if (B == this_block) continue;

    LLVMBBlock *our = CB[B];
    assert(our && "Don't have block constructed for PHI node");
    our->getLastNode()->addControlDependence(node);
  }
}

static void addControlDepsToPHIs(LLVMDependenceGraph *graph) {
  // some phi nodes just work like this
  //
  //  ; <label>:0
  //  %1 = load i32, i32* %a, align 4
  //  %2 = load i32, i32* %b, align 4
  //  %3 = icmp sgt i32 %1, %2
  //  br i1 %3, label %4, label %5
  //
  //  ; <label>:4                                       ; preds = %0
  //  br label %6
  //
  //  ; <label>:5                                       ; preds = %0
  //  br label %6
  //
  //  ; <label>:6                                       ; preds = %5, %4
  //  %p.0 = phi i32* [ %a, %4 ], [ %b, %5 ]
  //
  //  so we need to keep the blocks %5 and %6 even though it is empty

  // add control dependence to each block going to this phi
  // XXX: it is over-approximation, but we don't have nothing better now
  for (auto I = graph->begin(), E = graph->end(); I != E; ++I) {
    llvm::Value *val = I->first;
    if (llvm::PHINode *phi = llvm::dyn_cast<llvm::PHINode>(val)) {
      addControlDepsToPHI(graph, I->second, phi);
    }
  }
}

bool LLVMDependenceGraph::build(llvm::Function *func) {
  using namespace llvm;

  assert(func && "Passed no func");

  // do we have anything to process?
  if (func->size() == 0) return false;

  constructedFunctions.insert(make_pair(func, this));

  // create entry node
  LLVMNode *entry = new LLVMNode(func);
  addGlobalNode(entry);
  // we want the entry node to have this DG set
  entry->setDG(this);
  setEntry(entry);

  // add formal parameters to this graph
  addFormalParameters();

  // iterate over basic blocks
  BBlocksMapT &blocks = getBlocks();
  for (llvm::BasicBlock &llvmBB : *func) {
    LLVMBBlock *BB = build(llvmBB);
    blocks[&llvmBB] = BB;

    // first basic block is the entry BB
    if (!getEntryBB()) setEntryBB(BB);
  }

  assert(blocks.size() == func->size() && "Did not created all blocks");

  // add CFG edges
  for (auto &it : blocks) {
    BasicBlock *llvmBB = cast<BasicBlock>(it.first);
    LLVMBBlock *BB = it.second;
    BB->setDG(this);

    int idx = 0;
    for (succ_iterator S = succ_begin(llvmBB), SE = succ_end(llvmBB); S != SE;
         ++S) {
      LLVMBBlock *succ = blocks[*S];
      assert(succ && "Missing basic block");

      // don't let overflow the labels silently
      // if this ever happens, we need to change bit-size
      // of the label (255 is reserved for edge to
      // artificial single return value)
      if (idx >= 255) {
        errs() << "Too much of successors";
        abort();
      }

      BB->addSuccessor(succ, idx++);
    }
  }

  // if graph has no return inst, just create artificial exit node
  // and point there
  if (!getExit()) {
    assert(!unifiedExitBB && "We should not have exit BB");
    unifiedExitBB = std::unique_ptr<LLVMBBlock>(createSingleExitBB(this));
  }

  // check if we have everything
  assert(getEntry() && "Missing entry node");
  assert(getExit() && "Missing exit node");
  assert(getEntryBB() && "Missing entry BB");
  assert(getExitBB() && "Missing exit BB");

  addControlDepsToPHIs(this);

  // add CFG edge from entry point to the first instruction
  entry->addControlDependence(getEntryBB()->getFirstNode());

  return true;
}

bool LLVMDependenceGraph::build(llvm::Module *m, LLVMPointerAnalysis *pts,
                                LLVMReachingDefinitions *rda,
                                llvm::Function *entry, bool intra_procedural) {
  this->PTA = pts;
  this->RDA = rda;
  this->intraProcedural = intra_procedural;
  return build(m, entry);
}

void LLVMDependenceGraph::addFormalParameters() {
  using namespace llvm;

  LLVMNode *entryNode = getEntry();
  assert(entryNode && "No entry node when adding formal parameters");

  Function *func = dyn_cast<Function>(entryNode->getValue());
  assert(func && "entry node value is not a function");
  // assert(func->arg_size() != 0 && "This function is undefined?");
  if (func->arg_size() == 0) return;

  LLVMDGParameters *params = getOrCreateParameters();

  LLVMNode *in, *out;
  for (auto I = func->arg_begin(), E = func->arg_end(); I != E; ++I) {
    Value *val = (&*I);

    std::tie(in, out) = params->construct(val, val, this);
    assert(in && out);

    // add control edges
    entryNode->addControlDependence(in);
    entryNode->addControlDependence(out);
  }

  if (func->isVarArg()) {
    Value *val = ConstantPointerNull::get(func->getType());
    val->setName("vararg");
    in = new LLVMNode(val, true);
    out = new LLVMNode(val, true);
    in->setDG(this);
    out->setDG(this);

    params->setVarArg(in, out);
    entryNode->addControlDependence(in);
    entryNode->addControlDependence(out);
    in->addDataDependence(out);
  }
}

static bool array_match(llvm::StringRef name, const char *names[]) {
  unsigned idx = 0;
  while (names[idx]) {
    if (name.equals(names[idx])) return true;
    ++idx;
  }

  return false;
}

static bool array_match(llvm::StringRef name,
                        const std::vector<std::string> &names) {
  for (const auto &nm : names) {
    if (name == nm) return true;
  }

  return false;
}

static bool match_callsite_name(LLVMNode *callNode, const char *names[]) {
  using namespace llvm;

  // if the function is undefined, it has no subgraphs,
  // but is not called via function pointer
  if (!callNode->hasSubgraphs()) {
    const CallInst *callInst = cast<CallInst>(callNode->getValue());
    const Value *calledValue = callInst->getCalledValue();
    const Function *func = dyn_cast<Function>(calledValue->stripPointerCasts());
    // in the case we haven't run points-to analysis
    if (!func) return false;

    // otherwise we would have a subgraph
    assert(func->size() == 0);
    return array_match(func->getName(), names);
  } else {
    // simply iterate over the subgraphs, get the entry node
    // and check it
    for (LLVMDependenceGraph *dg : callNode->getSubgraphs()) {
      LLVMNode *entry = dg->getEntry();
      assert(entry && "No entry node in graph");

      const Function *func =
          cast<Function>(entry->getValue()->stripPointerCasts());
      if (array_match(func->getName(), names)) {
        return true;
      }
    }
  }

  return false;
}

static bool match_callsite_name(LLVMNode *callNode,
                                const std::vector<std::string> &names) {
  using namespace llvm;

  // if the function is undefined, it has no subgraphs,
  // but is not called via function pointer
  if (!callNode->hasSubgraphs()) {
    const CallInst *callInst = cast<CallInst>(callNode->getValue());
    const Value *calledValue = callInst->getCalledValue();
    const Function *func = dyn_cast<Function>(calledValue->stripPointerCasts());
    // in the case we haven't run points-to analysis
    if (!func) return false;

    return array_match(func->getName(), names);
  } else {
    // simply iterate over the subgraphs, get the entry node
    // and check it
    for (LLVMDependenceGraph *dg : callNode->getSubgraphs()) {
      LLVMNode *entry = dg->getEntry();
      assert(entry && "No entry node in graph");

      const Function *func =
          cast<Function>(entry->getValue()->stripPointerCasts());
      if (array_match(func->getName(), names)) {
        return true;
      }
    }
  }

  return false;
}

bool LLVMDependenceGraph::getCallSites(const char *name,
                                       std::set<LLVMNode *> *callsites) {
  const char *names[] = {name, NULL};
  return getCallSites(names, callsites);
}

bool LLVMDependenceGraph::getCallSites(const char *names[],
                                       std::set<LLVMNode *> *callsites) {
  for (auto &F : constructedFunctions) {
    for (auto &I : F.second->getBlocks()) {
      LLVMBBlock *BB = I.second;
      for (LLVMNode *n : BB->getNodes()) {
        if (llvm::isa<llvm::CallInst>(n->getValue())) {
          if (match_callsite_name(n, names)) callsites->insert(n);
        }
      }
    }
  }

  return callsites->size() != 0;
}

bool LLVMDependenceGraph::getCallSites(const std::vector<std::string> &names,
                                       std::set<LLVMNode *> *callsites) {
  for (const auto &F : constructedFunctions) {
    for (const auto &I : F.second->getBlocks()) {
      LLVMBBlock *BB = I.second;
      for (LLVMNode *n : BB->getNodes()) {
        if (llvm::isa<llvm::CallInst>(n->getValue())) {
          if (match_callsite_name(n, names)) callsites->insert(n);
        }
      }
    }
  }

  return callsites->size() != 0;
}

void LLVMDependenceGraph::computeControlExpression(bool addCDs) {
  LLVMCFABuilder builder;

  for (auto &F : getConstructedFunctions()) {
    llvm::Function *func = llvm::cast<llvm::Function>(F.first);
    LLVMCFA cfa = builder.build(*func);

    CE = cfa.compute();

    if (addCDs) {
      // compute the control scope
      CE.computeSets();
      auto &our_blocks = F.second->getBlocks();

      for (llvm::BasicBlock &B : *func) {
        LLVMBBlock *B1 = our_blocks[&B];

        // if this block is a predicate block,
        // we compute the control deps for it
        // XXX: for now we compute the control
        // scope, which is enough for slicing,
        // but may add some extra (transitive)
        // edges
        if (B.getTerminator()->getNumSuccessors() > 1) {
          auto CS = CE.getControlScope(&B);
          for (auto cs : CS) {
            assert(cs->isa(CENodeType::LABEL));
            auto lab = static_cast<CELabel<llvm::BasicBlock *> *>(cs);
            LLVMBBlock *B2 = our_blocks[lab->getLabel()];
            B1->addControlDependence(B2);
          }
        }
      }
    }
  }
}

void LLVMDependenceGraph::computeNonTerminationControlDependencies() {
  dg::cd::NonTerminationSensitiveControlDependencyAnalysis ntscdAnalysis(
      entryFunction, PTA);
  ntscdAnalysis.computeDependencies();
  auto dependencies = ntscdAnalysis.controlDependencies();

  for (const auto &node : dependencies) {
    if (!node.first->isArtificial()) {
      auto lastInstruction =
          findInstruction(castToLLVMInstruction(node.first->lastInstruction()),
                          getConstructedFunctions());
      for (const auto dependant : node.second) {
        for (const auto instruction : dependant->llvmInstructions()) {
          auto dgInstruction = findInstruction(
              castToLLVMInstruction(instruction), getConstructedFunctions());
          if (lastInstruction && dgInstruction) {
            lastInstruction->addControlDependence(dgInstruction);
          } else {
            static std::set<std::pair<LLVMNode *, LLVMNode *>> reported;
            if (reported.insert({lastInstruction, dgInstruction}).second) {
              llvm::errs() << "[CD] error: CD could not be set up, some "
                              "instruction was not found:\n";
              if (lastInstruction)
                llvm::errs()
                    << "[CD] last instruction: " << *lastInstruction->getValue()
                    << "\n";
              else
                llvm::errs() << "[CD] No last instruction\n";
              if (dgInstruction)
                llvm::errs() << "[CD] current instruction: "
                             << *dgInstruction->getValue() << "\n";
              else
                llvm::errs() << "[CD] No current instruction\n";
            }
          }
        }
        if (dependant->isExit() && lastInstruction) {
          auto noreturn = lastInstruction->getDG()->getOrCreateNoReturn();
          lastInstruction->addControlDependence(noreturn);
        }
      }
    }
  }
}

void LLVMDependenceGraph::computeInterferenceDependentEdges(
    ControlFlowGraph *controlFlowGraph) {
  auto regions = controlFlowGraph->threadRegions();
  MayHappenInParallel mayHappenInParallel(regions);

  for (const auto &currentRegion : regions) {
    auto llvmValuesForCurrentRegion = currentRegion->llvmInstructions();
    auto currentRegionLoads = getLoadInstructions(llvmValuesForCurrentRegion);
    auto currentRegionStores = getStoreInstructions(llvmValuesForCurrentRegion);
    auto parallelRegions = mayHappenInParallel.parallelRegions(currentRegion);
    for (const auto &parallelRegion : parallelRegions) {
      auto llvmInstructionsForParallelRegion =
          parallelRegion->llvmInstructions();
      auto parallelRegionLoads =
          getLoadInstructions(llvmInstructionsForParallelRegion);
      auto parallelRegionStores =
          getStoreInstructions(llvmInstructionsForParallelRegion);
      computeInterferenceDependentEdges(currentRegionLoads,
                                        parallelRegionStores);
      computeInterferenceDependentEdges(parallelRegionLoads,
                                        currentRegionStores);
    }
  }
}

void LLVMDependenceGraph::computeForkJoinDependencies(
    ControlFlowGraph *controlFlowGraph) {
  auto joins = controlFlowGraph->getJoins();
  for (const auto &join : joins) {
    auto joinNode =
        findInstruction(castToLLVMInstruction(join), constructedFunctions);
    for (const auto &fork : controlFlowGraph->getCorrespondingForks(join)) {
      auto forkNode =
          findInstruction(castToLLVMInstruction(fork), constructedFunctions);
      joinNode->addControlDependence(forkNode);
    }
  }
}

void LLVMDependenceGraph::computeCriticalSections(
    ControlFlowGraph *controlFlowGraph) {
  auto locks = controlFlowGraph->getLocks();
  for (auto lock : locks) {
    auto callLockInst = castToLLVMInstruction(lock);
    auto lockNode = findInstruction(callLockInst, constructedFunctions);
    auto correspondingNodes =
        controlFlowGraph->getCorrespondingCriticalSection(lock);
    for (auto correspondingNode : correspondingNodes) {
      auto node = castToLLVMInstruction(correspondingNode);
      auto dependentNode = findInstruction(node, constructedFunctions);
      if (dependentNode) {
        lockNode->addControlDependence(dependentNode);
      } else {
        llvm::errs() << "Instruction " << *dependentNode->getValue()
                     << " was not found, cannot setup"
                     << " control depency on lock\n";
      }
    }

    auto correspondingUnlocks = controlFlowGraph->getCorrespongingUnlocks(lock);
    for (auto unlock : correspondingUnlocks) {
      auto node = castToLLVMInstruction(unlock);
      auto unlockNode = findInstruction(node, constructedFunctions);
      if (unlockNode) {
        unlockNode->addControlDependence(lockNode);
      }
    }
  }
}

void LLVMDependenceGraph::computeInterferenceDependentEdges(
    const std::set<const llvm::Instruction *> &loads,
    const std::set<const llvm::Instruction *> &stores) {
  for (const auto &load : loads) {
    for (const auto &store : stores) {
      auto loadOperand = PTA->getPointsTo(load->getOperand(0));
      auto storeOperand = PTA->getPointsTo(store->getOperand(1));
      if (loadOperand && storeOperand) {  // if storeOperand does not have
                                          // pointsTo, expect it can write
                                          // anywhere??
        for (const auto pointerLoad : loadOperand->pointsTo) {
          for (const auto pointerStore : storeOperand->pointsTo) {
            if (pointerLoad.target == pointerStore.target &&
                (pointerLoad.offset.isUnknown() ||
                 pointerStore.offset.isUnknown() ||
                 pointerLoad.offset == pointerStore.offset)) {
              llvm::Instruction *loadInst =
                  const_cast<llvm::Instruction *>(load);
              llvm::Instruction *storeInst =
                  const_cast<llvm::Instruction *>(store);
              auto loadFunction = constructedFunctions.find(
                  const_cast<llvm::Function *>(load->getParent()->getParent()));
              auto storeFunction =
                  constructedFunctions.find(const_cast<llvm::Function *>(
                      store->getParent()->getParent()));
              if (loadFunction != constructedFunctions.end() &&
                  storeFunction != constructedFunctions.end()) {
                auto loadNode = loadFunction->second->findNode(loadInst);
                auto storeNode = storeFunction->second->findNode(storeInst);
                if (loadNode && storeNode) {
                  storeNode->addInterferenceDependence(loadNode);
                }
              }
            }
          }
        }
      }
    }
  }
}

std::set<const llvm::Instruction *> LLVMDependenceGraph::getLoadInstructions(
    const std::set<const llvm::Instruction *> &llvmInstructions) const {
  return getInstructionsOfType(llvm::Instruction::Load, llvmInstructions);
}

std::set<const llvm::Instruction *> LLVMDependenceGraph::getStoreInstructions(
    const std::set<const llvm::Instruction *> &llvmInstructions) const {
  return getInstructionsOfType(llvm::Instruction::Store, llvmInstructions);
}

std::set<const llvm::Instruction *> LLVMDependenceGraph::getInstructionsOfType(
    const unsigned opCode,
    const std::set<const llvm::Instruction *> &llvmInstructions) const {
  std::set<const llvm::Instruction *> instructions;
  for (const auto &llvmValue : llvmInstructions) {
    if (llvm::isa<llvm::Instruction>(llvmValue)) {
      const llvm::Instruction *instruction =
          static_cast<const llvm::Instruction *>(llvmValue);
      if (instruction->getOpcode() == opCode) {
        instructions.insert(instruction);
      }
    }
  }
  return instructions;
}

// the original algorithm from Ferrante & Ottenstein
// works with nodes that represent instructions, therefore
// there's no point in control dependence self-loops.
// However, we use basic blocks and having a 'node' control
// dependent on itself may be desired. If a block jumps
// on itself, the decision whether we get to that block (again)
// is made on that block - so we want to make it control dependent
// on itself.
void LLVMDependenceGraph::makeSelfLoopsControlDependent() {
  for (auto &F : getConstructedFunctions()) {
    auto &blocks = F.second->getBlocks();

    for (auto &it : blocks) {
      LLVMBBlock *B = it.second;

      if (B->successorsNum() > 1 && B->hasSelfLoop())
        // add self-loop control dependence
        B->addControlDependence(B);
    }
  }
}

void LLVMDependenceGraph::addNoreturnDependencies(LLVMNode *noret,
                                                  LLVMBBlock *from) {
  std::set<LLVMBBlock *> visited;
  ADT::QueueLIFO<LLVMBBlock *> queue;

  for (auto &succ : from->successors()) {
    if (visited.insert(succ.target).second) queue.push(succ.target);
  }

  while (!queue.empty()) {
    auto cur = queue.pop();

    // do the stuff
    for (auto node : cur->getNodes()) noret->addControlDependence(node);

    // queue successors
    for (auto &succ : cur->successors()) {
      if (visited.insert(succ.target).second) queue.push(succ.target);
    }
  }
}

void LLVMDependenceGraph::addNoreturnDependencies() {
  for (auto &F : getConstructedFunctions()) {
    auto &blocks = F.second->getBlocks();

    for (auto &it : blocks) {
      LLVMBBlock *B = it.second;
      std::set<LLVMNode *> noreturns;
      for (auto node : B->getNodes()) {
        // add dependencies for the found no returns
        for (auto nrt : noreturns) {
          nrt->addControlDependence(node);
        }

        if (auto params = node->getParameters()) {
          if (auto noret = params->getNoReturn()) {
            // process the rest of the block
            noreturns.insert(noret);

            // process reachable nodes
            addNoreturnDependencies(noret, B);
          }
        }
      }
    }
  }
}

void LLVMDependenceGraph::addDefUseEdges() {
  LLVMDefUseAnalysis DUA(this, RDA, PTA);
  DUA.run();
}

LLVMNode *findInstruction(llvm::Instruction *instruction,
                          const std::map<llvm::Value *, LLVMDependenceGraph *>
                              &constructedFunctions) {
  auto valueKey =
      constructedFunctions.find(instruction->getParent()->getParent());
  if (valueKey != constructedFunctions.end()) {
    return valueKey->second->findNode(instruction);
  }
  return nullptr;
}

llvm::Instruction *castToLLVMInstruction(const llvm::Value *value) {
  return const_cast<llvm::Instruction *>(
      static_cast<const llvm::Instruction *>(value));
}

}  // namespace dg
