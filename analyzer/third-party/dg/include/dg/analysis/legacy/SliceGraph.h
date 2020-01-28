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

    SliceNode (dg::LLVMNode *node, int node_depth){
      n = node;
      depth = node_depth;
    }

    void add_child(dg::LLVMNode *n, int depth){
      SliceNode *sn = new SliceNode(n, depth);
      //SliceNode sn = SliceNode(n, depth);
      //llvm::errs() << "added value of " << n << "\n";
      child_nodes.push_back(sn);
      //llvm::errs() << "added value of " << sn << "\n";
    }

    /*void search_and_add_child(dg::LLVMNode *n, int depth){
      llvm::errs() << "before ref myself\n";
      if(this->n == n){
        this->add_child(n, depth);
        llvm::errs() << "FOUND NODE ROOT\n";
        return;
      }
      llvm::errs() << "after ref myself\n";
      llvm::errs() << "size of children is " << this->child_nodes.size() << "empty" << this->child_nodes.empty() << "\n";
      for(child_iterator i = this->child_nodes.begin(); i != this->child_nodes.end(); ++i){
        SliceNode child_sn = *i;
        if(child_sn.n == n){
          llvm::errs() << "FOUND NODE\n";
          child_sn.add_child(n, depth);
          return;
        }
        child_sn.search_and_add_child(n, depth);
      }
    }*/
    SliceNode * search_children(dg::LLVMNode *n){
      //llvm::errs() << "before ref myself\n";
      if(this->n == n)
        return this;
      //llvm::errs() << "after ref myself\n";
      //llvm::errs() << "size of children is " << this->child_nodes.size() << "empty" << this->child_nodes.empty() << "\n";
      for(child_iterator i = this->child_nodes.begin(); i != this->child_nodes.end(); ++i){
        SliceNode *sn = *i;
        if(sn->n == n){
          //llvm::errs() << "FOUND NODE\n";
          return sn;
        }
        //llvm::errs() << "here1\n";
        SliceNode * out = sn->search_children(n);
       // llvm::errs() << "here2\n";
        if(out != nullptr)
          return out;
      }
      //llvm::errs() << "out\n";
      return nullptr;
    }

    int total_size(SliceNode *sn){
      int num = 0;
      num += sn->child_nodes.size();
      for(child_iterator i = sn->child_nodes.begin(); i != sn->child_nodes.end(); ++i){
        num += total_size(*i);
      }
      return num;
    }
    
   void print_graph(){
     llvm::errs() << "\n";
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
