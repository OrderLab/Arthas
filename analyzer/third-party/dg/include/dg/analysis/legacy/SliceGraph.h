#ifndef _SLICEGRAPH_H
#define _SLICEGRAPH_H

#include <queue>
#include <stack>
#include <utility>
#include <vector>

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/None.h"

#include "dg/llvm/LLVMNode.h"

using namespace std;

namespace llvm {
namespace slicegraph {

enum class SlicePersistence {
  Persistent,
  Volatile,
  Mixed
};

enum class SliceDirection {
  Backward,
  Forward,
  Full
};

class SliceNode{
  public:
    dg::LLVMNode * n;
    int depth;
    typedef vector<SliceNode *> children;
    typedef children::iterator child_iterator;
    children child_nodes;

    SliceNode (dg::LLVMNode *n, int depth){
      n = n;
      depth = depth;
    }

    void add_child(dg::LLVMNode *n, int depth){
      SliceNode *sn;
      sn->n = n;
      sn->depth = depth;
      child_nodes.push_back(sn);
    }

    SliceNode * search_children(dg::LLVMNode *n){
      if(this->n == n)
        return this;
      for(child_iterator i = child_nodes.begin(); i != child_nodes.end(); ++i){
        SliceNode *sn = *i;
        if(sn->n == n){
          llvm::errs() << "FOUND NODE\n";
          return sn;
        }
        SliceNode * out = sn->search_children(n);
        if(out != nullptr)
          return out;
      }
      return nullptr;
    }
};

class DGSlice{
  public:
    typedef llvm::SmallVector<dg::LLVMNode *, 20> Nodes;
    llvm::Instruction *fault_instruction;
    SliceDirection direction;
    SlicePersistence persistent_state;
    uint64_t slice_id;
};

class SliceGraph {
  public:
    typedef vector<SliceNode *> SliceNodeList;
    typedef SliceNodeList::iterator iterator;
    typedef SliceNodeList::const_iterator const_iterator;
    int maxDepth;
    SliceNode *root;

    bool compute_slices(){
      return false;
    }
    
    SliceNode * find_node(dg::LLVMNode *n){
      
      return nullptr;
    }

    bool add_node(){
      return false;
    }

    private:
      SliceNodeList nodeList;

};

}
}
#endif
