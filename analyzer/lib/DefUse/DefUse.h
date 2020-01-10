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
#include <utility>
#include <vector>

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/None.h"

#include "llvm/Support/raw_ostream.h"

namespace llvm {
namespace defuse {

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

   UserGraph(const Value *v, int depth = -1) {
     root = v;
     queue.push(v);
     queue.push(None);  // a dummy element marking end of a level
     int level = 0;
     while (!queue.empty()) {
       Optional<const Value *> head = queue.front();
       if (head != None) {
         const Value *elem = head.getValue();
         for (auto ui = elem->user_begin(); ui != elem->user_end(); ++ui) {
           if (const Instruction *inst = dyn_cast<Instruction>(*ui)) {
             if (const StoreInst *store = dyn_cast<StoreInst>(inst)) {
               // if this is a store instruction, it does not have users. we
               // need to find the users of the target (second operand) instead.
               if (const Instruction *op =
                       dyn_cast<Instruction>(store->getOperand(1))) {
                 if (visited.insert(op).second) {
                   queue.push(op);
                 }
               }
             } else {
               if (visited.insert(inst).second) {
                 queue.push(inst);
               }
             }
           }
         }
         // front is a regular element (User) that has not been visited before
         // put it into the user list
         if (level > 0) {
           const Value *user = head.getValue();
           if (!isa<AllocaInst>(user)) {
             // only if this is not an alloca instruction, which comes from
             // the getOperand handling of StoreInstruction.
             userList.push_back(UserNode(user, level));
           }
         }
       }
       queue.pop();  // remove the front element
       if (!queue.empty()) {
         head = queue.front();
         if (head != None) {
           continue;
         }
         // reached the marker, meaning we are about to go to the next level
         // check the level depth here so we may stop
         level += 1;
         queue.pop();
         if (depth > 0 && level > depth) {
           break;
         }
         if (!queue.empty()) {
           // if this is not the last, add a marker at the end of the queue
           queue.push(None);
         }
       }
     }
    }

    const_iterator begin() const { return userList.begin(); }
    const_iterator end() const { return userList.end(); }
    iterator begin() { return userList.begin(); }
    iterator end() { return userList.end(); }

   private:
    const Value* root;
    UserNodeList userList;
    VistedNodeSet visited;
    VisitQueue queue;
};

} // namespace defuse
} // namespace llvm

#endif /* __DEFUSE_H_ */
