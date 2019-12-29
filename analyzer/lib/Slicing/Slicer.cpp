//===- Slicer.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/DominanceFrontier.h"

using namespace llvm;

#define DEBUG_TYPE "hello"

STATISTIC(HelloCounter, "Counts number of functions greeted");


void writeCallLocator(Function &F){
   for (inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ++I){
      const Instruction *inst = &*I;
      if(inst->mayWriteToMemory()){
         errs() << *I << "\n";
      }
   }
}

namespace {
  // Hello - The first implementation, without getAnalysisUsage.
  struct Hello : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    Hello() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      ++HelloCounter;
      errs() << "Hello: ";
      errs().write_escaped(F.getName()) << '\n';
      return false;
    }
  };
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");

namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct Hello2 : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    Hello2() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      ++HelloCounter;
      errs() << "Hello: ";
      errs().write_escaped(F.getName()) << '\n';
      writeCallLocator(F);
      return false;
    }

    // We don't modify the program, so we preserve all analyses.
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
  };
}

char Hello2::ID = 0;
static RegisterPass<Hello2>
Y("hello2", "Hello World Pass (with getAnalysisUsage implemented)");

namespace {
   class Slicer : public ModulePass {
     public:
        static char ID;
        Slicer() : ModulePass(ID) {}

        bool runOnModule(Module &M){
           errs() << "beginning module slice\n";

	   //TODO: find init functions + proper order to iterate
	   for(Module::iterator F = M.begin(); F != M.end(); ++F){
	       errs() << "Function iteration of " << *F << "\n";
	   }
        }
        /*void getAnalysisUsage(AnalysisUsage &AU) const {
           AU.addRequired<PostDominatorTree>();
           AU.addRequired<PostDominanceFrontier>();
        }*/

   };
}

static RegisterPass<Slicer> Z("slice", "Slices the code");
char Slicer::ID = 0;

