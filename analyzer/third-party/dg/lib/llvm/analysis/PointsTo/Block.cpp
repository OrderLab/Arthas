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

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#if (__clang__)
#pragma clang diagnostic pop  // ignore -Wunused-parameter
#else
#pragma GCC diagnostic pop
#endif

#include "dg/llvm/analysis/PointsTo/PointerGraph.h"

namespace dg {
namespace analysis {
namespace pta {

void LLVMPointerGraphBuilder::addPHIOperands(const llvm::Function& F) {
  for (const llvm::BasicBlock& B : F) {
    for (const llvm::Instruction& I : B) {
      if (const llvm::PHINode* PHI = llvm::dyn_cast<llvm::PHINode>(&I)) {
        if (PSNode* node = getNodes(PHI)->getSingleNode())
          addPHIOperands(node, PHI);
      }
    }
  }
}

// return first and last nodes of the block
LLVMPointerGraphBuilder::PSNodesBlock
LLVMPointerGraphBuilder::buildPointerGraphBlock(const llvm::BasicBlock& block,
                                                PointerSubgraph* parent) {
  using namespace llvm;

  PSNodesBlock blk;

  for (const llvm::Instruction& Inst : block) {
    if (!isRelevantInstruction(Inst)) {
      // check if it is a zeroing of memory,
      // if so, set the corresponding memory to zeroed
      if (llvm::isa<llvm::MemSetInst>(&Inst)) checkMemSet(&Inst);

      continue;
    }

    assert(nodes_map.count(&Inst) == 0 && "Already built this instruction");

    if(const llvm::AtomicRMWInst *RMWI_n =
       dyn_cast<llvm::AtomicRMWInst>(&Inst)){
      llvm::AtomicRMWInst *RMWI = (llvm::AtomicRMWInst *)RMWI_n;
      Value *Ptr = RMWI->getPointerOperand();
      Value *Val = RMWI->getValOperand();
      IRBuilder<> Builder(RMWI);
      LoadInst *Orig = new LoadInst(Val->getType(), Ptr);
      //LoadInst *Orig = Builder.CreateLoad(Val->getType(), Ptr);
      const llvm::Instruction *constOrig = dyn_cast<const
       llvm::Instruction>(Orig);
      auto& seq1 = buildInstruction(*constOrig);
      if(seq1.invalid == 0){
        llvm::errs() << "ESCAPED\n";
        continue;
      }
      // set parent to the new nodes
      for (auto nd : seq1) {
          nd->setParent(parent);
      }
      blk.append(&seq1);
      Value *Res = nullptr;
      switch (RMWI->getOperation()){
         case llvm::AtomicRMWInst::Add:
         {
           Res = Builder.CreateAdd(Orig, Val);
           const llvm::Instruction *constRes = 
            dyn_cast<const llvm::Instruction>(Res);
           auto& seq2 = buildInstruction(*constRes);
           if(seq2.invalid == 0){
             llvm::errs() << "ESCAPED\n";
             continue;
           }
           // set parent to the new nodes
           for (auto nd : seq2) {
             nd->setParent(parent);
           }
           blk.append(&seq2);
           break;
         }
         default:
         {
           llvm::errs() << "Unsupported instruction : " << *RMWI << "\n";
           break;
         }
      }
      //Builder.CreateStore(Res, Ptr);
      StoreInst *str = new StoreInst(Res, Ptr);
      const llvm::Instruction *constStr = 
        dyn_cast<const llvm::Instruction>(str);
      auto& seq3 = buildInstruction(*constStr);
      if(seq3.invalid == 0){
        llvm::errs() << "ESCAPED\n";
        continue;
      }
      // set parent to the new nodes
      for (auto nd : seq3) {
         nd->setParent(parent);
       }
       blk.append(&seq3);
       break;
    }
 else{
    auto& seq = buildInstruction(Inst);
    if (seq.invalid == 0) {
      llvm::errs() << "ESCAPED\n";
      continue;
    }

    // set parent to the new nodes
    for (auto nd : seq) {
      nd->setParent(parent);
    }

    blk.append(&seq);
    }
  }

  return blk;
}

}  // namespace pta
}  // namespace analysis
}  // namespace dg
