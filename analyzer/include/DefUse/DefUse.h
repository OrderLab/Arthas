// The PMEM-Fault Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef __DEFUSE_H_
#define __DEFUSE_H_

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

#include "llvm/Support/raw_ostream.h"

namespace llvm {
namespace defuse {

enum class UserGraphWalkType {
  BFS,
  DFS
};

// User graph for a given instruction: it includes the direct users as well as
// the transitive closure of users' users etc.
class UserGraph {
  public:
   typedef std::pair<const Value *, int> UserNode;
   typedef std::vector<UserNode> UserNodeList;
   typedef UserNodeList::iterator iterator;
   typedef UserNodeList::const_iterator const_iterator;
   typedef SmallPtrSet<const Value *, 8> VistedNodeSet;
   typedef std::queue<Optional<const Value *>> VisitQueue;
   typedef std::stack<const Value *> VisitStack;

   UserGraph(const Value *v, UserGraphWalkType t=UserGraphWalkType::DFS, int depth=-1) {
     root = v;
     maxDepth = depth;
     if (t == UserGraphWalkType::DFS)
       doDFS();
     else
       doBFS();
   }

   const_iterator begin() const { return userList.begin(); }
   const_iterator end() const { return userList.end(); }
   iterator begin() { return userList.begin(); }
   iterator end() { return userList.end(); }

  protected:
   void doDFS() {
     visited.insert(root);
     visit_stack.push(root);
     while (!visit_stack.empty()) {
       const Value *elem = visit_stack.top();
       if (elem != root && !isa<AllocaInst>(elem)) {
         // skip alloca inst, which comes from the operand of StoreInstruction.
         userList.push_back(UserNode(elem, 1));
       }
       visit_stack.pop();
       processUser(elem, UserGraphWalkType::DFS);
     }
    }

    void doBFS() {
      visited.insert(root);
      visit_queue.push(root);
      visit_queue.push(None);  // a dummy element marking end of a level
      int level = 0;
      while (!visit_queue.empty()) {
        Optional<const Value *> head = visit_queue.front();
        if (head != None) {
          const Value *elem = head.getValue();
          if (elem != root && !isa<AllocaInst>(elem)) {
            // skip alloca inst, which comes from the operand of StoreInstruction.
            userList.push_back(UserNode(elem, level));
          }
          processUser(elem, UserGraphWalkType::BFS);
        }
        visit_queue.pop();  // remove the front element
        if (!visit_queue.empty()) {
          head = visit_queue.front();
          if (head != None) {
            continue;
          }
          // reached the marker, meaning we are about to go to the next level
          // check the level depth here so we may stop
          level += 1;
          visit_queue.pop();
          if (maxDepth > 0 && level > maxDepth) {
            break;
          }
          if (!visit_queue.empty()) {
            // if this is not the last, add a marker at the end of the visit_queue
            visit_queue.push(None);
          }
        }
      }
    }

  private:
   void processUser(const Value *elem, UserGraphWalkType walk) {
     for (auto ui = elem->user_begin(); ui != elem->user_end(); ++ui) {
       if (const Instruction *inst = dyn_cast<Instruction>(*ui)) {
         if (const StoreInst *store = dyn_cast<StoreInst>(inst)) {
           if (const Instruction *op = dyn_cast<Instruction>(store->getOperand(1))) {
             // if this is a store instruction, it does not have users. we
             // need to find the users of the target (second operand) instead.
             if (visited.insert(op).second) {
               if (walk == UserGraphWalkType::DFS)
                 visit_stack.push(op);
               else
                 visit_queue.push(op);
             }
           }
         } else {
           if (visited.insert(inst).second) {
             if (walk == UserGraphWalkType::DFS)
               visit_stack.push(inst);
             else
               visit_queue.push(inst);
           }
         }
       }
     }
   }

  private:
   int maxDepth;
   const Value *root;
   UserNodeList userList;
   VistedNodeSet visited;
   VisitQueue visit_queue;
   VisitStack visit_stack;
};

} // namespace defuse
} // namespace llvm

#endif /* __DEFUSE_H_ */
