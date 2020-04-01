// ignore unused parameters in LLVM libraries
#if (__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <vector>

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

    LLVMPointerGraphBuilder::PSNodesSeq* seq = nullptr;
    if (const llvm::AtomicRMWInst* RMWI =
            dyn_cast<llvm::AtomicRMWInst>(&Inst)) {
      llvm::errs() << "Atomic RMW Inst: " << Inst << "\n";
      seq = &buildAtomicRMWInst(const_cast<llvm::AtomicRMWInst*>(RMWI));
    } else if (const llvm::AtomicCmpXchgInst* CXI =
                   dyn_cast<llvm::AtomicCmpXchgInst>(&Inst)) {
      llvm::errs() << "Atomic CmpXchg Inst: " << Inst << "\n";
      seq = &buildAtomicCmpXchgInst(CXI);
    } else {
      seq = &buildInstruction(Inst);
    }

    if (!seq) continue;

    // set parent to the new nodes
    for (auto nd : *seq) {
      nd->setParent(parent);
    }

    blk.append(seq);
  }
  //}

  return blk;
}

}  // namespace pta
}  // namespace analysis
}  // namespace dg
