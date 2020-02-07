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

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#if (__clang__)
#pragma clang diagnostic pop // ignore -Wunused-parameter
#else
#pragma GCC diagnostic pop
#endif

#include "dg/llvm/analysis/PointsTo/PointerGraph.h"

namespace dg {
namespace analysis {
namespace pta {

void LLVMPointerGraphBuilder::addPHIOperands(const llvm::Function &F)
{
    for (const llvm::BasicBlock& B : F) {
        for (const llvm::Instruction& I : B) {
            if (const llvm::PHINode *PHI = llvm::dyn_cast<llvm::PHINode>(&I)) {
                if (PSNode *node = getNodes(PHI)->getSingleNode())
                    addPHIOperands(node, PHI);
            }
        }
    }
}

// return first and last nodes of the block
LLVMPointerGraphBuilder::PSNodesBlock
LLVMPointerGraphBuilder::buildPointerGraphBlock(const llvm::BasicBlock& block,
                                                PointerSubgraph *parent)
{
    using namespace llvm;

    PSNodesBlock blk;

    for (const llvm::Instruction& Inst : block) {

        if (!isRelevantInstruction(Inst)) {
            // check if it is a zeroing of memory,
            // if so, set the corresponding memory to zeroed
            if (llvm::isa<llvm::MemSetInst>(&Inst))
                checkMemSet(&Inst);

            continue;
        }

        assert(nodes_map.count(&Inst) == 0
                && "Already built this instruction");
    /*int problem = 0;
    switch(Inst.getOpcode()) {
        case Instruction::Alloca:
          break;
        case Instruction::Store:
          break;
        case Instruction::Load:
          break;
        case Instruction::GetElementPtr:
          break;
        case Instruction::ExtractValue:
          break;
        case Instruction::Select:
          break;
        case Instruction::PHI:
          break;
        case Instruction::BitCast:
          break;
        case Instruction::SExt:
          break;
        case Instruction::ZExt:
          break;
        case Instruction::PtrToInt:
          break;
        case Instruction::IntToPtr:
          break;
        case Instruction::Ret:
          break;
        case Instruction::Call:
          break;
        case Instruction::And:
          break;
        case Instruction::Or:
          break;
        case Instruction::Trunc:
          break;
        case Instruction::Shl:
          break;
        case Instruction::LShr:
          break;
        case Instruction::AShr:
          break;
        case Instruction::Xor:
          break;
        case Instruction::FSub:
          break;
        case Instruction::FAdd:
          break;
        case Instruction::FDiv:
          break;
        case Instruction::FMul:
          break;
        case Instruction::UDiv:
          break;
        case Instruction::SDiv:
          break;
        case Instruction::URem:
          break;
        case Instruction::SRem:
          break;
        case Instruction::FRem:
          break;
        case Instruction::FPTrunc:
          break;
        case Instruction::FPExt:
          break;
        case Instruction::Add:
          break;
        case Instruction::Sub:
          break;
        case Instruction::Mul:
          break;
        case Instruction::UIToFP:
          break;
        case Instruction::SIToFP:
          break;
        case Instruction::FPToUI:
          break;
        case Instruction::FPToSI:
          break;
        case Instruction::InsertElement:
          break;
        case Instruction::ExtractElement:
          break;
        case Instruction::ShuffleVector:
          break;
        default:
          llvm::errs() << "ONLY HERE \n";
          problem = 1;
          break;
    }

        if(problem)
          continue;*/

        auto& seq = buildInstruction(Inst);
        if(seq.invalid == 0){
          llvm::errs() << "ESCAPED\n";
          continue;
        }


        // set parent to the new nodes
        for (auto nd : seq) {
            nd->setParent(parent);
        }

        blk.append(&seq);
    }

    return blk;
}

} // namespace pta
} // namespace analysis
} // namespace dg
