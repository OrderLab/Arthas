//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Details are in a white paper by F. Tip called:
// A survey of program slicing techniques
//===----------------------------------------------------------------------===//

#include <cctype>
#include <map>

#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "Callgraph/Callgraph.h"
#include "Languages/LLVMSupport.h"
#include "Modifies/Modifies.h"
#include "PointsTo/PointsTo.h"
#include "Slicing/PostDominanceFrontier.h"
#include "Slicing/FunctionStaticSlicer.h"

using namespace llvm;
using namespace llvm::slicing;

static uint64_t getSizeOfMem(const Value *val) {
  if (const ConstantInt *CI = dyn_cast<ConstantInt>(val)) {
    // 2016.2.25
    // after LLVM opt, the 3rd argument of memset is -2 when testing coreutils
    // numfmt.c
    if (!CI->isNegative()) {
      return CI->getLimitedValue();
    }
  } else if (const Constant *C = dyn_cast<Constant>(val)) {
    if (C->isNullValue())
      return 0;
    assert(0 && "unknown constant");
  }

  /* This sucks indeed, it is only a wild guess... */
  // return 64; // 2016-03-28
  return 32;
}

void InsInfo::addDEFArray(const ptr::PointsToSets &PS, const Value *V,
                          uint64_t lenConst) {
  if (isPointerValue(V)) {
    typedef ptr::PointsToSets::PointsToSet PTSet;

    const PTSet &L = getPointsToSet(V, PS);
    for (PTSet::const_iterator p = L.begin(); p != L.end(); ++p)
      for (uint64_t i = 0; i < lenConst; i++)
        addDEF(Pointee(p->first, p->second + i));
  }
}

void InsInfo::handleVariousFuns(const ptr::PointsToSets &PS, const CallInst *C,
                                const Function *F) {
  if (!F->hasName())
    return;

  StringRef fName = F->getName();

  if (fName.equals("klee_make_symbolic")) {
    const Value *l = elimConstExpr(C->getArgOperand(0));
    const Value *len = elimConstExpr(C->getArgOperand(1));
    uint64_t lenConst = getSizeOfMem(len);

    addREF(Pointee(l, -1));
    addDEFArray(PS, l, lenConst);
    if (!isConstantValue(len))
      addREF(Pointee(len, -1));
  }
}

void InsInfo::addREFArray(const ptr::PointsToSets &PS, const Value *V,
                          uint64_t lenConst) {
  if (isPointerValue(V)) {
    typedef ptr::PointsToSets::PointsToSet PTSet;

    const PTSet &R = getPointsToSet(V, PS);
    for (PTSet::const_iterator p = R.begin(); p != R.end(); ++p)
      for (uint64_t i = 0; i < lenConst; i++)
        addREF(Pointee(p->first, p->second + i));
  }
}

namespace {
bool _hasVarIndex(const GetElementPtrInst &gep) {
  // OPTIMIZE: std::any_of
  for (unsigned i = 1, e = gep.getNumOperands(); i != e; ++i) {
    // or Constant::classof(&gep) ?
    if (!isConstantValue(gep.getOperand(i))) {
      return true;
    }
  }

  return false;
}
}

// 2016.01.14 by jiangg
void InsInfo::_analyseCallInst(shape::RWType type, const CallInst &C,
                               const ptr::PointsToSets &PS,
                               const mods::Modifies &MOD) {
  if (type == shape::RWType::STATE_CACHE) {
    // 2015.11.3 by jiangg
    // intrapro
    if (!callToVoidFunction(&C)) {
      addDEF(Pointee(&C, -1));
    }

    return;
  }

  // for -enable-slice
  const Value *cv = C.getCalledValue();
  if (isInlineAssembly(&C)) {
    // errs() << "ERROR: Inline assembler detected in " <<
    //   C.getParent()->getParent()->getName() << ", ignoring\n";
  } else if (isMemoryAllocation(cv)) {
    if (!isConstantValue(C.getArgOperand(0)))
      addREF(Pointee(C.getArgOperand(0), -1));
    addDEF(Pointee(&C, -1));
  } else if (isMemoryDeallocation(cv)) {
  } else if (isMemoryCopy(cv) || isMemoryMove(cv) || isMemorySet(cv)) {
    const Value *len = elimConstExpr(C.getArgOperand(2));
    uint64_t lenConst = getSizeOfMem(len);

    const Value *l = elimConstExpr(C.getOperand(0));
    addDEFArray(PS, l, lenConst);
    addREF(Pointee(l, -1));

    const Value *r = elimConstExpr(C.getOperand(1));
    /* memset has a constant/variable there */
    if (isMemoryCopy(cv) || isMemoryMove(cv))
      addREFArray(PS, r, lenConst);
    addREF(Pointee(r, -1));

    /* memcpy/memset wouldn't work with len being 'undef' */
    if (!isConstantValue(len))
      addREF(Pointee(len, -1));
  } else {
    /* did we miss something? */
    assert(!memoryManStuff(cv));

    if (const Function *F = dyn_cast<Function>(cv))
      handleVariousFuns(PS, &C, F);
    else
      addREF(Pointee(cv, -1));

    std::vector<const Function *> calledVec;
    getCalledFunctions(&C, PS, std::back_inserter(calledVec));
    for (const Function *F : calledVec) {
      const mods::Modifies::ModSet &M = getModSet(F, MOD);
      for (const ptr::PointsToSets::Pointee &pointee : M) {
        // 2016.01.14
        // addDEF(pointee);
        // TODO:
        // int arr[3];
        // foo(arr + 2); // only modify arr[2] in foo
        addDEF({pointee.first, -1});
        // errs() << "**** add def " << pointee.second << ",";
        // pointee.first->dump();
      }
    }

    // if callee has no body definition, do conservative analyse
    // for pointer arguments
    // TODO: first analyse library functions exactly
    // PS.dump();
    if (const Function *F = C.getCalledFunction()) {
      // F has no body definition
      if (F->isDeclaration()) {
        // errs() << "F declaration is " << F->getName() << '\n';
        for (Value *actual_arg : C.arg_operands()) {
          // 2016.2.23

          // void foo(int *b);
          //
          // int main() {
          //   int a = 9;
          //   int *p = &a;
          //   if (a > 0) {
          //     foo(&a);
          //     foo(p);
          //   }
          //
          //   return a;
          // }
          if (AllocaInst::classof(actual_arg)) {
            // %a = alloca i32, align 4
            // call void @foo(i32* %a) // C program: foo(&a);
            // declare void @foo(i32*) #2
            addDEF({actual_arg, -1});
          } else {
            // %a = alloca i32, align 4
            // store i32* %a, i32** %p, align 8, !dbg !17
            // %p = alloca i32*, align 8
            // %1 = load i32** %p, align 8, !dbg !23
            // call void @foo(i32* %1), !dbg !24 // C program: foo(p);
            // declare void @foo(i32*) #2

            // arg->dump();
            const ptr::PointsToSets::PointsToSet &S =
                ptr::getPointsToSet(actual_arg, PS); // key is <-1, actual_arg>
            // errs() << "S size is " << S.size();
            for (const ptr::PointsToSets::Pointee &pointee : S) {
              addDEF({pointee.first, -1});
            }
          }
        }
      }
    }

    if (!callToVoidFunction(&C))
      addDEF(Pointee(&C, -1));
  }
}

// 2015.10.20 by jiangg
// for RWset analysis (liveness analysis)
InsInfo::InsInfo(shape::RWType type, const Instruction *i,
                 const ptr::PointsToSets &PS, const mods::Modifies &MOD)
    : ins(i) {
  typedef ptr::PointsToSets::PointsToSet PTSet;

  if (const LoadInst *LI = dyn_cast<const LoadInst>(i)) {
    addDEF(Pointee(i, -1));

    const Value *op = elimConstExpr(LI->getPointerOperand());
    if (isa<ConstantPointerNull>(op)) {
      errs() << "ERROR in analysed code -- reading from address 0 at "
             << i->getParent()->getParent()->getName() << ":\n";
      i->print(errs());
    } else if (!isa<ConstantInt>(op)) {
      //   %2 = load i32* %i, align 4
      // DEF:
      // OFF is -1   %2 = load i32* %i, align 4
      // REF:
      // OFF is -1   %i = alloca i32, align 4
      // OFF is 0   %i = alloca i32, align 4 // remove

      // addREF(Pointee(op, -1));
      if (hasExtraReference(op)) {
        // 2015.10.23 by jiangg
        // change 0 to -1
        addREF(Pointee(op, -1));
      } else {
        const PTSet &S = getPointsToSet(op, PS);
        for (PTSet::const_iterator I = S.begin(), E = S.end(); I != E; ++I) {
          Pointee pointee = *I;
          // same as Store
          if (GetElementPtrInst::classof(op)) {
            if (_hasVarIndex(cast<GetElementPtrInst>(*op))) {
              pointee.second = -1;
            }
          } else {
            pointee.second = -1;
          }
          addREF(pointee);
        }
      }
    }
  } else if (const StoreInst *SI = dyn_cast<const StoreInst>(i)) {
    // store rightValue, leftValue (a pointer)
    const Value *l = elimConstExpr(SI->getPointerOperand());
    if (isa<ConstantPointerNull>(l)) {
      errs() << "ERROR in analysed code -- writing to address 0 at "
             << i->getParent()->getParent()->getName() << ":\n";
      i->print(errs());
    } else if (!isa<ConstantInt>(l)) {
      if (hasExtraReference(l)) { // AllocaInst, GlobalVariable or Function
        // 2015.10.21
        // e.g. store i32 2, i32* %a
        // def: in IR %a is a pointer, so offset is 0 to get whose content,
        // but in source code(our interface), offset is -1.
        // addDEF(Pointee(l, 0)); // change 0 to -1
        addDEF(Pointee(l, -1));
      } else {
        const PTSet &S = getPointsToSet(l, PS);
        for (PTSet::const_iterator I = S.begin(), E = S.end(); I != E; ++I) {
          Pointee pointee = *I;

          if (GetElementPtrInst::classof(l)) {
            if (_hasVarIndex(cast<GetElementPtrInst>(*l))) {
              // hack LLVMSlicer's bug in points_to_sets
              // l is %arrayidx

              // **********************************
              //   %arrayidx = getelementptr inbounds [3 x i32]* %a, i32 0, i64
              //   %idxprom
              // DEF:
              // OFF is -1   %arrayidx = getelementptr inbounds [3 x i32]* %a,
              // i32 0, i64 %idxprom
              // REF:
              // OFF is -1   %a = alloca [3 x i32], align 4 // remove
              // OFF is -1   %idxprom = sext i32 %1 to i64
              // **********************************
              //   store i32 %0, i32* %arrayidx, align 4
              // DEF:
              // OFF is 0   %a = alloca [3 x i32], align 4 // should be -1
              // REF:
              // OFF is -1   %arrayidx = getelementptr inbounds [3 x i32]* %a,
              // i32 0, i64 %idxprom // remove
              // OFF is -1   %0 = load i32* %b, align 4
              // **********************************
              pointee.second = -1;
            }
          } else {
            // 2016.1.15
            // e.g.
            // 1) for function argument
            // define i32 @foo(i32 %a) #0

            // entry:
            //   %a.addr = alloca i32, align 4
            //   %p = alloca i32*, align 8
            //
            //   %1 = load i32** %p, align 8, !dbg !29
            //   store i32 3, i32* %1, align 4, !dbg !31

            // 2)
            // int a;
            // foo(&a); // modify a in foo
            pointee.second = -1;
          }
          addDEF(pointee);
        }
      }

      //   store i32 %conv8, i32* %x, align 4
      // DEF:
      // OFF is 0   %x = alloca i32, align 4
      // REF:
      // OFF is -1   %x = alloca i32, align 4 // remove
      // OFF is -1   %conv8 = sext i8 %4 to i32

      // if (!l->getType()->isIntegerTy())
      // 	addREF(Pointee(l, -1));

      const Value *r = elimConstExpr(SI->getValueOperand());
      if (!hasExtraReference(r) && !isConstantValue(r)) {
        addREF(Pointee(r, -1));
      }
    }
  } else if (const GetElementPtrInst *gep =
                 dyn_cast<const GetElementPtrInst>(i)) {
    addDEF(Pointee(i, -1));

    // removed by jiangg 2015.10.22
    // no need to add the total aggregate variable to ref
    // addREF(Pointee(gep->getPointerOperand(), -1));

    for (unsigned i = 1, e = gep->getNumOperands(); i != e; ++i) {
      Value *op = gep->getOperand(i);
      if (!isConstantValue(op))
        addREF(Pointee(op, -1));
    }
  } else if (CallInst const *const C = dyn_cast<const CallInst>(i)) {
    _analyseCallInst(type, *C, PS, MOD);
  } else if (isa<const ReturnInst>(i)) {
  } else if (const BinaryOperator *BO = dyn_cast<const BinaryOperator>(i)) {
    addDEF(Pointee(i, -1));

    if (!isConstantValue(BO->getOperand(0)))
      addREF(Pointee(BO->getOperand(0), -1));
    if (!isConstantValue(BO->getOperand(1)))
      addREF(Pointee(BO->getOperand(1), -1));
  } else if (const CastInst *CI = dyn_cast<const CastInst>(i)) {
    addDEF(Pointee(i, -1));

    if (!hasExtraReference(CI->getOperand(0)))
      addREF(Pointee(CI->getOperand(0), -1));
  } else if (const AllocaInst *AI = dyn_cast<const AllocaInst>(i)) {
    addDEF(Pointee(AI, -1));
    if (!isConstantValue(AI->getArraySize()))
      addREF(Pointee(AI->getArraySize(), -1));
  } else if (const CmpInst *CI = dyn_cast<const CmpInst>(i)) {
    addDEF(Pointee(i, -1));

    if (!isConstantValue(CI->getOperand(0)))
      addREF(Pointee(CI->getOperand(0), -1));
    if (!isConstantValue(CI->getOperand(1)))
      addREF(Pointee(CI->getOperand(1), -1));
  } else if (const BranchInst *BI = dyn_cast<const BranchInst>(i)) {
    if (BI->isConditional() && !isConstantValue(BI->getCondition()))
      addREF(Pointee(BI->getCondition(), -1));
  } else if (const PHINode *phi = dyn_cast<const PHINode>(i)) {
    addDEF(Pointee(i, -1));

    for (unsigned k = 0; k < phi->getNumIncomingValues(); ++k)
      if (!isConstantValue(phi->getIncomingValue(k)))
        addREF(Pointee(phi->getIncomingValue(k), -1));
  } else if (const SwitchInst *SI = dyn_cast<SwitchInst>(i)) {
    if (!isConstantValue(SI->getCondition()))
      addREF(Pointee(SI->getCondition(), -1));
  } else if (const SelectInst *SI = dyn_cast<const SelectInst>(i)) {
    // TODO: THE FOLLOWING CODE HAS NOT BEEN TESTED YET

    addDEF(Pointee(i, -1));

    if (!isConstantValue(SI->getCondition()))
      addREF(Pointee(SI->getCondition(), -1));
    if (!isConstantValue(SI->getTrueValue()))
      addREF(Pointee(SI->getTrueValue(), -1));
    if (!isConstantValue(SI->getFalseValue()))
      addREF(Pointee(SI->getFalseValue(), -1));
  } else if (isa<const UnreachableInst>(i)) {
  } else if (const ExtractValueInst *EV = dyn_cast<const ExtractValueInst>(i)) {
    addDEF(Pointee(i, -1));
    addREF(Pointee(EV->getAggregateOperand(), -1));
  } else if (const InsertValueInst *IV = dyn_cast<const InsertValueInst>(i)) {
    //      TODO THE FOLLOWING CODE HAS NOT BEEN TESTED YET

    const Value *r = IV->getInsertedValueOperand();
    addDEF(Pointee(IV->getAggregateOperand(), -1));
    if (!isConstantValue(r))
      addREF(Pointee(r, -1));
  } else {
    errs() << "ERROR: Unsupported instruction reached\n";
    i->print(errs());
  }
}

InsInfo::InsInfo(const Instruction *i, const ptr::PointsToSets &PS,
                 const mods::Modifies &MOD)
    : ins(i) {
  typedef ptr::PointsToSets::PointsToSet PTSet;

  if (const LoadInst *LI = dyn_cast<const LoadInst>(i)) {
    addDEF(Pointee(i, -1));

    const Value *op = elimConstExpr(LI->getPointerOperand());
    if (isa<ConstantPointerNull>(op)) {
      errs() << "ERROR in analysed code -- reading from address 0 at "
             << i->getParent()->getParent()->getName() << ":\n";
      i->print(errs());
    } else if (!isa<ConstantInt>(op)) {
      addREF(Pointee(op, -1));
      if (hasExtraReference(op)) {
        addREF(Pointee(op, 0));
      } else {
        const PTSet &S = getPointsToSet(op, PS);
        for (PTSet::const_iterator I = S.begin(), E = S.end(); I != E; ++I) {
          addREF(*I);
        }
      }
    }
  } else if (const StoreInst *SI = dyn_cast<const StoreInst>(i)) {
    const Value *l = elimConstExpr(SI->getPointerOperand());
    if (isa<ConstantPointerNull>(l)) {
      errs() << "ERROR in analysed code -- writing to address 0 at "
             << i->getParent()->getParent()->getName() << ":\n";
      i->print(errs());
    } else if (!isa<ConstantInt>(l)) {
      if (hasExtraReference(l)) {
        addDEF(Pointee(l, 0));
      } else {
        const PTSet &S = getPointsToSet(l, PS);
        for (PTSet::const_iterator I = S.begin(), E = S.end(); I != E; ++I) {
          addDEF(*I);
        }
      }

      if (!l->getType()->isIntegerTy())
        addREF(Pointee(l, -1));
      const Value *r = elimConstExpr(SI->getValueOperand());
      if (!hasExtraReference(r) && !isConstantValue(r))
        addREF(Pointee(r, -1));
    }
  } else if (const GetElementPtrInst *gep =
                 dyn_cast<const GetElementPtrInst>(i)) {
    addDEF(Pointee(i, -1));

    addREF(Pointee(gep->getPointerOperand(), -1));

    for (unsigned i = 1, e = gep->getNumOperands(); i != e; ++i) {
      Value *op = gep->getOperand(i);
      if (!isConstantValue(op))
        addREF(Pointee(op, -1));
    }
  } else if (CallInst const *const C = dyn_cast<const CallInst>(i)) {
    const Value *cv = C->getCalledValue();

    if (isInlineAssembly(C)) {
      errs() << "ERROR: Inline assembler detected in "
             << i->getParent()->getParent()->getName() << ", ignoring\n";
    } else if (isMemoryAllocation(cv)) {
      if (!isConstantValue(C->getArgOperand(0)))
        addREF(Pointee(C->getArgOperand(0), -1));
      addDEF(Pointee(i, -1));
    } else if (isMemoryDeallocation(cv)) {
    } else if (isMemoryCopy(cv) || isMemoryMove(cv) || isMemorySet(cv)) {
      const Value *len = elimConstExpr(C->getArgOperand(2));
      uint64_t lenConst = getSizeOfMem(len);

      const Value *l = elimConstExpr(C->getOperand(0));
      addDEFArray(PS, l, lenConst);
      addREF(Pointee(l, -1));

      const Value *r = elimConstExpr(C->getOperand(1));
      /* memset has a constant/variable there */
      if (isMemoryCopy(cv) || isMemoryMove(cv))
        addREFArray(PS, r, lenConst);
      addREF(Pointee(r, -1));

      /* memcpy/memset wouldn't work with len being 'undef' */
      if (!isConstantValue(len))
        addREF(Pointee(len, -1));
    } else {
      typedef std::vector<const llvm::Function *> CalledVec;

      /* did we miss something? */
      assert(!memoryManStuff(cv));

      if (const Function *F = dyn_cast<Function>(cv))
        handleVariousFuns(PS, C, F);
      else
        addREF(Pointee(cv, -1));

      CalledVec CV;
      getCalledFunctions(C, PS, std::back_inserter(CV));
      for (CalledVec::const_iterator f = CV.begin(); f != CV.end(); ++f) {
        mods::Modifies::mapped_type const &M = getModSet(*f, MOD);
        for (mods::Modifies::mapped_type::const_iterator v = M.begin();
             v != M.end(); ++v) {
          addDEF(*v);
        }
      }

      if (!callToVoidFunction(C))
        addDEF(Pointee(C, -1));
    }
  } else if (isa<const ReturnInst>(i)) {
  } else if (const BinaryOperator *BO = dyn_cast<const BinaryOperator>(i)) {
    addDEF(Pointee(i, -1));

    if (!isConstantValue(BO->getOperand(0)))
      addREF(Pointee(BO->getOperand(0), -1));
    if (!isConstantValue(BO->getOperand(1)))
      addREF(Pointee(BO->getOperand(1), -1));
  } else if (const CastInst *CI = dyn_cast<const CastInst>(i)) {
    addDEF(Pointee(i, -1));

    if (!hasExtraReference(CI->getOperand(0)))
      addREF(Pointee(CI->getOperand(0), -1));
  } else if (const AllocaInst *AI = dyn_cast<const AllocaInst>(i)) {
    addDEF(Pointee(AI, -1));
    if (!isConstantValue(AI->getArraySize()))
      addREF(Pointee(AI->getArraySize(), -1));
  } else if (const CmpInst *CI = dyn_cast<const CmpInst>(i)) {
    addDEF(Pointee(i, -1));

    if (!isConstantValue(CI->getOperand(0)))
      addREF(Pointee(CI->getOperand(0), -1));
    if (!isConstantValue(CI->getOperand(1)))
      addREF(Pointee(CI->getOperand(1), -1));
  } else if (const BranchInst *BI = dyn_cast<const BranchInst>(i)) {
    if (BI->isConditional() && !isConstantValue(BI->getCondition()))
      addREF(Pointee(BI->getCondition(), -1));
  } else if (const PHINode *phi = dyn_cast<const PHINode>(i)) {
    addDEF(Pointee(i, -1));

    for (unsigned k = 0; k < phi->getNumIncomingValues(); ++k)
      if (!isConstantValue(phi->getIncomingValue(k)))
        addREF(Pointee(phi->getIncomingValue(k), -1));
  } else if (const SwitchInst *SI = dyn_cast<SwitchInst>(i)) {
    if (!isConstantValue(SI->getCondition()))
      addREF(Pointee(SI->getCondition(), -1));
  } else if (const SelectInst *SI = dyn_cast<const SelectInst>(i)) {
    // TODO: THE FOLLOWING CODE HAS NOT BEEN TESTED YET

    addDEF(Pointee(i, -1));

    if (!isConstantValue(SI->getCondition()))
      addREF(Pointee(SI->getCondition(), -1));
    if (!isConstantValue(SI->getTrueValue()))
      addREF(Pointee(SI->getTrueValue(), -1));
    if (!isConstantValue(SI->getFalseValue()))
      addREF(Pointee(SI->getFalseValue(), -1));
  } else if (isa<const UnreachableInst>(i)) {
  } else if (const ExtractValueInst *EV = dyn_cast<const ExtractValueInst>(i)) {
    addDEF(Pointee(i, -1));
    addREF(Pointee(EV->getAggregateOperand(), -1));
  } else if (const InsertValueInst *IV = dyn_cast<const InsertValueInst>(i)) {
    //      TODO THE FOLLOWING CODE HAS NOT BEEN TESTED YET

    const Value *r = IV->getInsertedValueOperand();
    addDEF(Pointee(IV->getAggregateOperand(), -1));
    if (!isConstantValue(r))
      addREF(Pointee(r, -1));
  } else {
    errs() << "ERROR: Unsupported instruction reached\n";
    i->print(errs());
  }
}

namespace {
class FunctionSlicer : public ModulePass {
public:
  static char ID;

  FunctionSlicer() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<PostDominatorTreeWrapperPass>();
    AU.addRequired<PostDominanceFrontier>();
  }

private:
  bool runOnFunction(Function &F, const ptr::PointsToSets &PS,
                     const mods::Modifies &MOD);
};
}

static RegisterPass<FunctionSlicer> X("slice", "Slices the code");
char FunctionSlicer::ID;

FunctionStaticSlicer::~FunctionStaticSlicer() {
  for (InsInfoMap::const_iterator I = insInfoMap.begin(), E = insInfoMap.end();
       I != E; I++)
    delete I->second;
}

typedef llvm::SmallVector<const Instruction *, 10> SuccList;

static SuccList getSuccList(const Instruction *i) {
  SuccList succList;
  const BasicBlock *bb = i->getParent();
  if (i != &bb->back()) {
    BasicBlock::const_iterator I(i);
    I++;
    succList.push_back(&*I);
  } else {
    for (succ_const_iterator I = succ_begin(bb), E = succ_end(bb); I != E; I++)
      succList.push_back(&(*I)->front());
  }
  return succList;
}

bool FunctionStaticSlicer::sameValues(const Pointee &val1,
                                      const Pointee &val2) {
  return val1.first == val2.first && val1.second == val2.second;
}

/*
 * RC(i)=RC(i) \cup
 *   {v| v \in RC(j), v \notin DEF(i)} \cup
 *   {v| v \in REF(i), DEF(i) \cap RC(j) \neq \emptyset}
 */
bool FunctionStaticSlicer::computeRCi(InsInfo *insInfoi, InsInfo *insInfoj) {
  bool changed = false;

  /* {v| v \in RC(j), v \notin DEF(i)} */
  for (ValSet::const_iterator I = insInfoj->RC_begin(), E = insInfoj->RC_end();
       I != E; I++) {
    const Pointee &RCj = *I;
    bool in_DEF = false;
    for (ValSet::const_iterator II = insInfoi->DEF_begin(),
                                EE = insInfoi->DEF_end();
         II != EE; II++)
      if (sameValues(*II, RCj)) {
        in_DEF = true;
        break;
      }
    if (!in_DEF)
      if (insInfoi->addRC(RCj))
        changed = true;
  }
  /* DEF(i) \cap RC(j) \neq \emptyset */
  bool isect_nonempty = false;
  for (ValSet::const_iterator I = insInfoi->DEF_begin(),
                              E = insInfoi->DEF_end();
       I != E && !isect_nonempty; I++) {
    const Pointee &DEFi = *I;
    for (ValSet::const_iterator II = insInfoj->RC_begin(),
                                EE = insInfoj->RC_end();
         II != EE; II++) {
      if (sameValues(DEFi, *II)) {
        isect_nonempty = true;
        break;
      }
    }
  }

  /* {v| v \in REF(i), ...} */
  if (isect_nonempty)
    for (ValSet::const_iterator I = insInfoi->REF_begin(),
                                E = insInfoi->REF_end();
         I != E; I++)
      if (insInfoi->addRC(*I))
        changed = true;
#ifdef DEBUG_RC
  errs() << "  " << __func__ << "2 END";
  if (changed)
    errs() << " ----------CHANGED";
  errs() << '\n';
#endif
  return changed;
}

bool FunctionStaticSlicer::computeRCi(InsInfo *insInfoi) {
  const Instruction *i = insInfoi->getIns();
  bool changed = false;
#ifdef DEBUG_RC
  errs() << "  " << __func__ << ": " << i->getOpcodeName();
  if (i->hasName())
    errs() << " (" << i->getName() << ")";
  errs() << '\n';
  errs() << "    DUMP: ";
  i->print(errs());
  errs() << '\n';
#endif
  SuccList succList = getSuccList(i);
  for (SuccList::const_iterator I = succList.begin(), E = succList.end();
       I != E; I++)
    changed |= computeRCi(insInfoi, getInsInfo(*I));

  return changed;
}

void FunctionStaticSlicer::computeRC() {
  bool changed;
#ifdef DEBUG_RC
  int it = 1;
#endif
  do {
    changed = false;
#ifdef DEBUG_RC
    errs() << __func__ << ": ============== Iteration " << it++ << '\n';
#endif
    typedef std::reverse_iterator<Function::iterator> revFun;
    for (revFun I = revFun(fun.end()), E = revFun(fun.begin()); I != E; I++) {
      typedef std::reverse_iterator<BasicBlock::iterator> rev;
      InsInfo *past = nullptr;
      for (rev II = rev(I->end()), EE = rev(I->begin()); II != EE; ++II) {
        InsInfo *insInfo = getInsInfo(&*II);
        if (!past)
          changed |= computeRCi(insInfo);
        else
          changed |= computeRCi(insInfo, past);
        past = insInfo;
      }
    }
  } while (changed);
}

/*
 * SC(i)={i| DEF(i) \cap RC(j) \neq \emptyset}
 */
void FunctionStaticSlicer::computeSCi(const Instruction *i,
                                      const Instruction *j) {
  InsInfo *insInfoi = getInsInfo(i), *insInfoj = getInsInfo(j);

  bool isect_nonempty = false;
  for (ValSet::const_iterator I = insInfoi->DEF_begin(),
                              E = insInfoi->DEF_end();
       I != E && !isect_nonempty; I++) {
    const Pointee &DEFi = *I;
    for (ValSet::const_iterator II = insInfoj->RC_begin(),
                                EE = insInfoj->RC_end();
         II != EE; II++) {
      if (sameValues(DEFi, *II)) {
        isect_nonempty = true;
        break;
      }
    }
  }

  if (isect_nonempty) {
    insInfoi->deslice();
#ifdef DEBUG_SLICING
    errs() << "XXXXXXXXXXXXXY ";
    i->print(errs());
    errs() << '\n';
#endif
  }
}

void FunctionStaticSlicer::computeSC() {
  for (inst_iterator I = inst_begin(fun), E = inst_end(fun); I != E; I++) {
    const Instruction *i = &*I;
    SuccList succList = getSuccList(i);
    for (SuccList::const_iterator II = succList.begin(), EE = succList.end();
         II != EE; II++)
      computeSCi(i, *II);
  }
}

bool FunctionStaticSlicer::computeBC() {
  bool changed = false;
#ifdef DEBUG_BC
  errs() << __func__ << " ============ BEG\n";
#endif
  PostDominanceFrontier &PDF = MP->getAnalysis<PostDominanceFrontier>(fun);
  for (inst_iterator I = inst_begin(fun), E = inst_end(fun); I != E; I++) {
    Instruction *i = &*I;
    const InsInfo *ii = getInsInfo(i);
    if (ii->isSliced())
      continue;
    BasicBlock *BB = i->getParent();
#ifdef DEBUG_BC
    errs() << "  ";
    i->print(errs());
    errs() << " -> bb=" << BB->getName() << '\n';
#endif
    PostDominanceFrontier::const_iterator frontier = PDF.find(BB);
    if (frontier == PDF.end())
      continue;
    changed |= updateRCSC(frontier->second.begin(), frontier->second.end());
  }
#ifdef DEBUG_BC
  errs() << __func__ << " ============ END\n";
#endif
  return changed;
}

bool FunctionStaticSlicer::updateRCSC(
    PostDominanceFrontier::DomSetType::const_iterator start,
    PostDominanceFrontier::DomSetType::const_iterator end) {
  bool changed = false;
#ifdef DEBUG_RC
  errs() << __func__ << " ============ BEG\n";
#endif
  for (; start != end; start++) {
    const BasicBlock *BB = *start;
    const Instruction &i = BB->back();
    InsInfo *ii = getInsInfo(&i);
/* SC = BC \cup ... */
#ifdef DEBUG_SLICING
    errs() << "XXXXXXXXXXXXXX " << BB->getName() << " ";
    i.print(errs());
    errs() << '\n';
#endif
    ii->deslice();
    /* RC = ... \cup \cup(b \in BC) RB */
    for (ValSet::const_iterator II = ii->REF_begin(), EE = ii->REF_end();
         II != EE; II++)
      if (ii->addRC(*II)) {
        changed = true;
#ifdef DEBUG_RC
        errs() << "  added " << (*II)->getName() << "\n";
#endif
      }
  }
#ifdef DEBUG_RC
  errs() << __func__ << " ============ END: changed=" << changed << "\n";
#endif
  return changed;
}

static bool canSlice(const Instruction &i) {
  switch (i.getOpcode()) {
  case Instruction::Alloca:
  case Instruction::Ret:
  case Instruction::Unreachable:
    return false;
  case Instruction::Br:
  case Instruction::Switch:
    return false;
  }
  return true;
}

void FunctionStaticSlicer::dump() {
#ifdef DEBUG_DUMP
  for (inst_iterator I = inst_begin(fun), E = inst_end(fun); I != E; I++) {
    const Instruction &i = *I;
    const InsInfo *ii = getInsInfo(&i);
    i.print(errs());
    errs() << "\n    ";
    if (!ii->isSliced() || !canSlice(i))
      errs() << "UN";
    errs() << "SLICED\n    DEF:\n";
    for (ValSet::const_iterator II = ii->DEF_begin(), EE = ii->DEF_end();
         II != EE; II++) {
      errs() << "      OFF=" << II->second << " ";
      II->first->dump();
    }
    errs() << "    REF:\n";
    for (ValSet::const_iterator II = ii->REF_begin(), EE = ii->REF_end();
         II != EE; II++) {
      errs() << "      OFF=" << II->second << " ";
      II->first->dump();
    }
    errs() << "    RC:\n";
    for (ValSet::const_iterator II = ii->RC_begin(), EE = ii->RC_end();
         II != EE; II++) {
      errs() << "      OFF=" << II->second << " ";
      II->first->dump();
    }
  }
#endif
}

/**
 * this method calculates the static slice for the CFG
 */
void FunctionStaticSlicer::calculateStaticSlice() {
#ifdef DEBUG_SLICE
  errs() << __func__ << " ============ BEG\n";
#endif
  do {
#ifdef DEBUG_SLICE
    errs() << __func__ << " ======= compute RC\n";
#endif
    computeRC();
#ifdef DEBUG_SLICE
    errs() << __func__ << " ======= compute SC\n";
#endif
    computeSC();

#ifdef DEBUG_SLICE
    errs() << __func__ << " ======= compute BC\n";
#endif
  } while (computeBC());

  dump();

#ifdef DEBUG_SLICE
  errs() << __func__ << " ============ END\n";
#endif
}

bool FunctionStaticSlicer::slice() {
#ifdef DEBUG_SLICE
  errs() << __func__ << " ============ BEG\n";
#endif
  bool removed = false;
  for (inst_iterator I = inst_begin(fun), E = inst_end(fun); I != E;) {
    Instruction &i = *I;
    InsInfoMap::iterator ii_iter = insInfoMap.find(&i);
    assert(ii_iter != insInfoMap.end());
    const InsInfo *ii = ii_iter->second;
    ++I;
    if (ii->isSliced() && canSlice(i)) {
#ifdef DEBUG_SLICE
      errs() << "  removing:";
      i.print(errs());
      errs() << " from " << i.getParent()->getName() << '\n';
#endif
      i.replaceAllUsesWith(UndefValue::get(i.getType()));
      i.eraseFromParent();
      insInfoMap.erase(ii_iter);
      delete ii;

      removed = true;
    }
  }
  return removed;
}

/**
 * removeUndefBranches -- remove branches with undef condition
 *
 * These are irrelevant to the code, so may be removed completely with their
 * bodies.
 */
void FunctionStaticSlicer::removeUndefBranches(ModulePass *MP, Function &F) {
#ifdef DEBUG_SLICE
  errs() << __func__ << " ============ Removing unused branches\n";
#endif
  PostDominatorTree &PDT = MP->getAnalysis<PostDominatorTreeWrapperPass>(F).getPostDomTree();
  typedef llvm::SmallVector<const BasicBlock *, 10> Unsafe;
  Unsafe unsafe;

  for (Function::iterator I = F.begin(), E = F.end(); I != E; ++I) {
    BasicBlock &bb = *I;
    if (std::distance(succ_begin(&bb), succ_end(&bb)) <= 1)
      continue;
    Instruction &back = bb.back();
    if (back.getOpcode() != Instruction::Br &&
        back.getOpcode() != Instruction::Switch)
      continue;
    const Value *cond = back.getOperand(0);
    if (cond->getValueID() != Value::UndefValueVal)
      continue;
    DomTreeNode *node = PDT.getNode(&bb);
    if (!node) /* this bb is unreachable */
      continue;
    DomTreeNode *idom = node->getIDom();
    assert(idom);
    /*    if (!idom)
          continue;*/
    BasicBlock *dest = idom->getBlock();
    if (!dest) /* TODO when there are nodes with noreturn calls */
      continue;
#ifdef DEBUG_SLICE
    errs() << "  considering branch: " << bb.getName() << '\n';
    errs() << "  dest=" << dest->getName() << "\n";
#endif
    if (PHINode *PHI = dyn_cast<PHINode>(&dest->front()))
      if (PHI->getBasicBlockIndex(&bb) == -1) {
        /* TODO this is unsafe! */
        unsafe.push_back(&bb);
        PHI->addIncoming(Constant::getNullValue(PHI->getType()), &bb);
      }
    BasicBlock::iterator ii(back);
    Instruction *newI = BranchInst::Create(dest);
    ReplaceInstWithInst(bb.getInstList(), ii, newI);
  }
  for (Unsafe::const_iterator I = unsafe.begin(), E = unsafe.end(); I != E;
       ++I) {
    const BasicBlock *bb = *I;
    if (std::distance(pred_begin(bb), pred_end(bb)) > 1)
      errs() << "WARNING: PHI node with added value which is zero\n";
  }
#ifdef DEBUG_SLICE
  errs() << __func__ << " ============ END\n";
#endif
}

/**
 * removeUndefCalls -- remove calls with undef function
 *
 * These are irrelevant to the code, so may be removed completely.
 */
void FunctionStaticSlicer::removeUndefCalls(ModulePass * /*MP*/, Function &F) {
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E;) {
    CallInst *CI = dyn_cast<CallInst>(&*I);
    ++I;
    if (CI && isa<UndefValue>(CI->getCalledValue())) {
      CI->replaceAllUsesWith(UndefValue::get(CI->getType()));
      CI->eraseFromParent();
    }
  }
}

void FunctionStaticSlicer::removeUndefs(ModulePass *MP, Function &F) {
  removeUndefBranches(MP, F);
  removeUndefCalls(MP, F);
}

static bool handleAssert(Function &F, FunctionStaticSlicer &ss,
                         const CallInst *CI) {
  const char *ass_file = getenv("SLICE_ASSERT_FILE");
  const char *ass_line = getenv("SLICE_ASSERT_LINE");
  const ConstantExpr *fileArg = dyn_cast<ConstantExpr>(CI->getArgOperand(1));
  const ConstantInt *lineArg = dyn_cast<ConstantInt>(CI->getArgOperand(2));

  if (ass_file && ass_line) {
    if (fileArg && fileArg->getOpcode() == Instruction::GetElementPtr &&
        lineArg) {
      const GlobalVariable *strVar =
          dyn_cast<GlobalVariable>(fileArg->getOperand(0));
      assert(strVar && strVar->hasInitializer());
      const ConstantDataArray *str =
          dyn_cast<ConstantDataArray>(strVar->getInitializer());
      assert(str && str->isCString());
      /* trim the NUL terminator */
      StringRef fileArgStr = str->getAsString().drop_back(1);
      const int ass_line_int = atoi(ass_line);

      errs() << "ASSERT at " << fileArgStr << ":" << lineArg->getValue()
             << "\n";

      if (fileArgStr.equals(ass_file) && lineArg->equalsInt(ass_line_int)) {
        errs() << "\tMATCH\n";
        goto count;
      }
    }
    ss.addSkipAssert(CI);
    return false;
  }

count:
#ifdef DEBUG_INITCRIT
  errs() << "    adding\n";
#endif

  const Value *aif =
      F.getParent()->getGlobalVariable("__ai_init_functions", true);
  ss.addInitialCriterion(CI, ptr::PointsToSets::Pointee(aif, -1));

  return true;
}

bool llvm::slicing::findInitialCriterion(Function &F, FunctionStaticSlicer &ss,
                                         bool starting) {
  bool added = false;
#ifdef DEBUG_INITCRIT
  errs() << __func__ << " ============ BEGIN\n";
#endif
  const Function *Fklee_assume = F.getParent()->getFunction("klee_assume");
  const Function *F__assert_fail = F.getParent()->getFunction("__assert_fail");
  if (!F__assert_fail) /* no cookies in this module */
    return false;

  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    const Instruction *i = &*I;
    if (const StoreInst *SI = dyn_cast<StoreInst>(i)) {
      const Value *LHS = SI->getPointerOperand();
      if (LHS->hasName() && LHS->getName().startswith("__ai_state_")) {
#ifdef DEBUG_INITCRIT
        errs() << "    adding\n";
#endif
        ss.addInitialCriterion(SI, ptr::PointsToSets::Pointee(LHS, -1));
      }
    } else if (const CallInst *CI = dyn_cast<CallInst>(i)) {
      Function *callie = CI->getCalledFunction();
      if (callie == F__assert_fail) {
        added = handleAssert(F, ss, CI);
      } else if (callie == Fklee_assume) { // this is kind of hack
        const Value *l = elimConstExpr(CI->getArgOperand(0));
        ss.addInitialCriterion(CI, ptr::PointsToSets::Pointee(l, -1));
      }
    } else if (const ReturnInst *RI = dyn_cast<ReturnInst>(i)) {
      if (starting) {
        const Module *M = F.getParent();
        for (Module::const_global_iterator II = M->global_begin(),
                                           EE = M->global_end();
             II != EE; ++II) {
          const GlobalVariable &GV = *II;
          if (!GV.hasName() || !GV.getName().startswith("__ai_state_"))
            continue;
#ifdef DEBUG_INITCRIT
          errs() << "adding " << GV.getName() << " into " << F.getName()
                 << " to \n";
          RI->dump();
#endif
          ss.addInitialCriterion(RI, ptr::PointsToSets::Pointee(&GV, -1),
                                 false);
        }
      }
    }
  }
#ifdef DEBUG_INITCRIT
  errs() << __func__ << " ============ END\n";
#endif
  return added;
}

bool FunctionSlicer::runOnFunction(Function &F, const ptr::PointsToSets &PS,
                                   const mods::Modifies &MOD) {
  FunctionStaticSlicer ss(F, this, PS, MOD);

  findInitialCriterion(F, ss);

  ss.calculateStaticSlice();

  bool sliced = ss.slice();
  if (sliced)
    FunctionStaticSlicer::removeUndefs(this, F);

  return sliced;
}

bool FunctionSlicer::runOnModule(Module &M) {
  ptr::PointsToSets PS;
  {
    ptr::ProgramStructure P(M);
    computePointsToSets(P, PS);
  }

  callgraph::Callgraph CG(M, PS);

  mods::Modifies MOD;
  {
    mods::ProgramStructure P1(M);
    computeModifies(P1, CG, PS, MOD);
  }

  bool modified = false;
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration())
      modified |= runOnFunction(F, PS, MOD);
  }
  return modified;
}
