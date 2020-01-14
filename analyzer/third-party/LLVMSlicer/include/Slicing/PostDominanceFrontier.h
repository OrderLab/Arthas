#ifndef POST_DOMINANCE_FRONTIER
#define POST_DOMINANCE_FRONTIER

#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/InitializePasses.h"

namespace llvm {

struct CreateHammockCFG : public FunctionPass {
  static char ID;

  CreateHammockCFG() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
  }
};

//===-------------------------------------
/// PostDominanceFrontierBase Class - Concrete subclass of DominanceFrontierBase
/// that is
/// used to compute a post dominator frontiers.
///
template <class BlockT>
class PostDominanceFrontierBase : public DominanceFrontierBase<BlockT> {
private:
  using BlockTraits = GraphTraits<BlockT *>;

public:
  // using DomTreeT = DominatorTreeBase<BlockT>;	// why not Post
  using DomTreeNodeT = DomTreeNodeBase<BlockT>;
  using DomSetType = typename DominanceFrontierBase<BlockT>::DomSetType;

  PostDominanceFrontierBase() : DominanceFrontierBase<BlockT>(false) {}

  void analyze(PostDominatorTree &postDominatorTree) {
#ifdef CONTROL_DEPENDENCE_GRAPH
    // TODO: upgrade
    calculate(postDominatorTree, F); // F is from caller runOnFunction(F)
#else
    this->Roots = postDominatorTree.getRoots();
    if (const DomTreeNode *Root = postDominatorTree.getRootNode()) {
      calculate(postDominatorTree, Root);
#ifdef PDF_DUMP
      errs() << "=== DUMP:\n";
      dump();
      errs() << "=== EOD\n";
#endif
    }
#endif
  }

#ifdef CONTROL_DEPENDENCE_GRAPH
  typedef std::pair<DomTreeNode *, DomTreeNode *> Ssubtype;
  typedef std::set<Ssubtype> Stype;

  void calculate(const PostDominatorTree &DT, Function &F);
  void constructS(const PostDominatorTree &DT, Function &F, Stype &S);
  const DomTreeNode *findNearestCommonDominator(const PostDominatorTree &DT,
                                                DomTreeNode *A, DomTreeNode *B);
#else
  const DomSetType &calculate(const PostDominatorTree &DT,
                              const DomTreeNodeT *Node);
#endif
};

class PostDominanceFrontier : public FunctionPass {
private:
  PostDominanceFrontierBase<BasicBlock> Base;

public:
  using DomSetType = DominanceFrontierBase<BasicBlock>::DomSetType;
  using iterator = DominanceFrontierBase<BasicBlock>::iterator;
  using const_iterator = DominanceFrontierBase<BasicBlock>::const_iterator;

  static char ID;

  PostDominanceFrontier() : FunctionPass(ID), Base() {
    initializeDominanceFrontierWrapperPassPass(*PassRegistry::getPassRegistry());
  }

  iterator begin() { return Base.begin(); }

  const_iterator begin() const { return Base.begin(); }

  iterator end() { return Base.end(); }

  const_iterator end() const { return Base.end(); }

  iterator find(BasicBlock *pBasicBlock) { return Base.find(pBasicBlock); }

  const_iterator find(BasicBlock *pBasicBlock) const {
    return Base.find(pBasicBlock);
  }

  void releaseMemory() override { Base.releaseMemory(); }

  bool runOnFunction(Function &) override {
    releaseMemory();
    Base.analyze(getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree());
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<PostDominatorTreeWrapperPass>();
  }
};
}

#endif
