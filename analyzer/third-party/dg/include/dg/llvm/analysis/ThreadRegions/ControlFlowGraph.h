#ifndef CONTROLFLOWGRAPH_H
#define CONTROLFLOWGRAPH_H

#include <memory>
#include <set>

// ignore unused parameters in LLVM libraries
#if (__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <llvm/IR/Instructions.h>

#if (__clang__)
#pragma clang diagnostic pop // ignore -Wunused-parameter
#else
#pragma GCC diagnostic pop
#endif

namespace dg {
    class LLVMPointerAnalysis;
}

namespace llvm {
    class Function;
}

class ThreadRegion;
class GraphBuilder;
class ThreadRegionsBuilder;
class CriticalSectionsBuilder;

class ControlFlowGraph
{
private:
    std::unique_ptr<GraphBuilder>               graphBuilder;
    std::unique_ptr<ThreadRegionsBuilder>       threadRegionsBuilder;
    std::unique_ptr<CriticalSectionsBuilder>    criticalSectionsBuilder;

public:
    ControlFlowGraph(dg::LLVMPointerAnalysis * pointsToAnalysis);

    ~ControlFlowGraph();

    std::set<const llvm::CallInst *> getJoins() const;

    std::set<const llvm::CallInst *> getCorrespondingForks(const llvm::CallInst * callInst) const;

    std::set<const llvm::CallInst *> getLocks() const;

    std::set<const llvm::CallInst *> getCorrespongingUnlocks(const llvm::CallInst * callInst) const;

    std::set<const llvm::Instruction *> getCorrespondingCriticalSection(const llvm::CallInst * callInst) const;

    void buildFunction(const llvm::Function *function);

    void printWithRegions(std::ostream & ostream) const;

    void printWithoutRegions(std::ostream & ostream) const;

    std::set<ThreadRegion *> threadRegions();
};

#endif // CONTROLFLOWGRAPH_H
