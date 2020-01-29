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

class DGSlice{
  public:
    std::list<dg::LLVMNode *> nodes;
    std::list<llvm::Instruction *> dependentInstructions;
    llvm::Instruction *fault_instruction;
    SliceDirection direction;
    SlicePersistence persistent_state;
    uint64_t slice_id;
    dg::LLVMNode * highest_node;

    DGSlice() {}

    DGSlice (llvm::Instruction *fi, SliceDirection direction, 
    SlicePersistence persistent_state, uint64_t slice_id, dg::LLVMNode *highest_node){
      fault_instruction = fi;
      direction = direction;
      persistent_state = persistent_state;
      slice_id = slice_id;
      highest_node = highest_node;
    }

   void print_dgslice(){
     //llvm::errs() << "Slice " << slice_id << " \n";
     for(std::list<dg::LLVMNode *>::iterator i = nodes.begin(); i != nodes.end(); ++i){
       llvm::errs() << *i << "\n";
     }
   }

   void get_values(list<llvm::Instruction *> pmem_list){
     bool vol = false;
     bool persistent = false;
     for(std::list<dg::LLVMNode *>::iterator i = nodes.begin(); i != nodes.end(); ++i){
       dg::LLVMNode *n = *i;
       llvm::Value *v = n->getValue();
       llvm::Instruction *inst = dyn_cast<llvm::Instruction>(v);
       dependentInstructions.push_back(inst);
       for(std::list<llvm::Instruction *>::iterator pmem_i = pmem_list.begin(); 
       pmem_i != pmem_list.end(); ++pmem_i){
         if(inst == *pmem_i){
           persistent = true;
         }
         else{
           vol = true;
         }
       }
     }
     if(vol && persistent){
       persistent_state = SlicePersistence::Mixed;
     }
     else if(vol){
       persistent_state = SlicePersistence::Volatile; 
     }
     else if(persistent){
       persistent_state = SlicePersistence::Persistent;
     }
   }
};


class SliceNode{
  public:
    dg::LLVMNode * n;
    int depth;
    typedef vector<SliceNode *> children;
    typedef children::iterator child_iterator;
    children child_nodes;
    dg::LLVMNode * prev_node;

    SliceNode (dg::LLVMNode *node, int node_depth){
      n = node;
      depth = node_depth;
    }

    void add_child(dg::LLVMNode *n, int depth, dg::LLVMNode *prev_node){
      SliceNode *sn = new SliceNode(n, depth);
      llvm::errs() << "added value of " << n << "\n";
      sn->prev_node = prev_node;
      child_nodes.push_back(sn);
      //llvm::errs() << "added value of " << sn << "\n";
    }

    SliceNode * search_children(dg::LLVMNode *n){
      if(this->n == n)
        return this;
      for(child_iterator i = this->child_nodes.begin(); i != this->child_nodes.end(); ++i){
        SliceNode *sn = *i;
        if(sn->n == n){
          return sn;
        }
        SliceNode * out = sn->search_children(n);
        if(out != nullptr)
          return out;
      }
      return nullptr;
    }

    int total_size(SliceNode *sn){
      int num = 0;
      num++;
      for(child_iterator i = sn->child_nodes.begin(); i != sn->child_nodes.end(); ++i){
        num += total_size(*i);
      }
      return num;
    }
    
   void print_graph(){
     llvm::Value *v = this->n->getValue();
     //llvm::errs() << *v << "\n";
     llvm::errs() << this->n << "\n";
     if(!(this->child_nodes.empty()))
       llvm::errs() << "children: \n";
     else{
       llvm::errs() << "branch end \n";
     }
     for(child_iterator i = this->child_nodes.begin(); i != this->child_nodes.end(); ++i){
       SliceNode *sn = *i;
       sn->print_graph();
     }
    }

   void print(int level){
     if(level == 0){
       llvm::errs() << "root " << this->n << "\n";
     }

     for(child_iterator i = this->child_nodes.begin(); i != this->child_nodes.end(); ++i){
       SliceNode *sn = *i;
       llvm::errs() << sn->n << " depth of " << level << "prev is " << this->n << "prev double check is " << sn->prev_node << "\n";
     }
     int new_level = level + 1;
     for(child_iterator i = this->child_nodes.begin(); i != this->child_nodes.end(); ++i){
       SliceNode *sn = *i;
       sn->print(new_level);
     }
   }

   void slice_node_copy(DGSlice &base, list<DGSlice>& slices){
     DGSlice *s;
     for(std::list<DGSlice>::iterator i = slices.begin(); i != slices.end(); ++i){
        s = &*i;
        //if(this->prev_node == s->highest_node || slices.size() == 1){
        if(this->n == s->highest_node || slices.size() == 1){
          llvm::errs() << "prev node is " << this->prev_node << "current node is " << this->n << "\n";
          llvm::errs() << "s highest nodes is " << s->highest_node << "\n";
          //llvm::errs() << "before copy "<< s->nodes.size()  << "\n";
          base.nodes = s->nodes;
          //llvm::errs() << "after copy "<< s->nodes.size()  << "\n";
          llvm::errs() << "Found slice\n";
          break;
        }
      }
 
   }
   int compute_slices(list<DGSlice>& slices, llvm::Instruction *fi, SliceDirection sd,
    SlicePersistence sp, uint64_t slice_id){
      uint64_t slice_num = slice_id;
      //Base Case: Adding in a slice of just the root node of the graph
      if(slices.empty()){
        DGSlice dgs = DGSlice(fi, sd, sp, slice_id, this->n );
        dgs.nodes.push_back(this->n);
        dgs.highest_node = this->n;
        slices.push_back(dgs);
        slice_num++;
      }

      //Creating base slice to copy, need to change slice_id and highest_node
      //DGSlice base = DGSlice(fi, sd, sp, slice_num, this->n);
      DGSlice *s;
      for(std::list<DGSlice>::iterator i = slices.begin(); i != slices.end(); ++i){
        s = &*i;
        //if(this->prev_node == s->highest_node || slices.size() == 1){
        if(this->n == s->highest_node || slices.size() == 1){
          llvm::errs() << "prev node is " << this->prev_node << "current node is " << this->n << "\n";
          llvm::errs() << "s highest nodes is " << s->highest_node << "\n";
          //llvm::errs() << "before copy "<< s->nodes.size()  << "\n";
          //base.nodes = s->nodes;
          //llvm::errs() << "after copy "<< s->nodes.size()  << "\n";
          llvm::errs() << "Found slice\n";
          break;
        }
      }

      /*if(base.nodes.size() == 0)
        llvm::errs() << "problem: prev node is " << this->prev_node << "current node is " << this->n << "\n";
      */

      int children_num = this->child_nodes.size();
      int child_count = 0;
      //Iterating through all of the children to create more slices
     for(child_iterator i = this->child_nodes.begin(); i != this->child_nodes.end(); ++i){
       SliceNode *sn = *i;
       child_count++;
       llvm::errs() << "child of " << this->n << " is " << sn->n << "\n";
       llvm::errs() << "child_count is " << child_count << " children count is " << children_num << "\n";
       if(child_count == children_num){
         llvm::errs() << "pushing to existing branch\n";
         s->nodes.push_back(sn->n);
         s->highest_node = sn->n;
         llvm::errs() << "number of slices is " << slices.size() << "\n";
       }
       else{
         DGSlice base = DGSlice(fi, sd, sp, slice_num, this->n);
         this->slice_node_copy(base, slices);
         llvm::errs() << "new slice old highest is " << base.highest_node << "\n";
         llvm::errs() << "base nodes size is " << base.nodes.size() << "\n";
         base.slice_id = slice_num;
         base.highest_node = sn->n;
         base.nodes.push_back(sn->n);
         slice_num++;
         slices.push_back(base);
         llvm::errs() << "number of slices is " << slices.size() << "\n";
       }
       slice_num = sn->compute_slices(slices, fi, sd, sp, slice_num);
     }
     return slice_num;
   }
};

class SliceGraph {
  public:
    SliceNode *root;
    typedef vector<SliceNode *> SliceNodeList;
    typedef SliceNodeList::iterator iterator;
    typedef SliceNodeList::const_iterator const_iterator;
    int maxDepth;

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
