#ifndef _LLVM_DG_RD_H_
#define _LLVM_DG_RD_H_

#include <unordered_map>
#include <memory>
#include <type_traits>

// ignore unused parameters in LLVM libraries
#if (__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <llvm/Support/raw_os_ostream.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

#if (__clang__)
#pragma clang diagnostic pop // ignore -Wunused-parameter
#else
#pragma GCC diagnostic pop
#endif

#include "dg/analysis/ReachingDefinitions/ReachingDefinitions.h"
#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"
#include "dg/llvm/analysis/ReachingDefinitions/LLVMReachingDefinitionsAnalysisOptions.h"

namespace dg {
namespace analysis {
namespace rd {

class LLVMRDBuilder;

class LLVMReachingDefinitions
{
    LLVMRDBuilder *builder{nullptr};
    std::unique_ptr<ReachingDefinitionsAnalysis> RDA;
    const llvm::Module *m;
    dg::LLVMPointerAnalysis *pta;
    const LLVMReachingDefinitionsAnalysisOptions _options;

    void initializeSparseRDA();
    void initializeDenseRDA();

public:

    LLVMReachingDefinitions(const llvm::Module *m,
                            dg::LLVMPointerAnalysis *pta,
                            const LLVMReachingDefinitionsAnalysisOptions& opts)
        : m(m), pta(pta), _options(opts) {}

    ~LLVMReachingDefinitions();

    /**
     * Template parameters:
     * RdaType - class extending dg::analysis::rd::ReachingDefinitions to be used as analysis
     */
    template <typename RdaType>
    void run()
    {
        // this helps while guessing causes of template substitution errors
        static_assert(std::is_base_of<ReachingDefinitionsAnalysis, RdaType>::value,
                      "RdaType has to be subclass of ReachingDefinitionsAnalysis");

        if (std::is_same<RdaType, SSAReachingDefinitionsAnalysis>::value) {
            initializeSparseRDA();
        } else {
            initializeDenseRDA();
        }

        assert(builder);
        assert(RDA);
        assert(getRoot());

        RDA->run();
    }

    RDNode *getRoot() { return RDA->getRoot(); }
    ReachingDefinitionsGraph *getGraph() { return RDA->getGraph(); }
    RDNode *getNode(const llvm::Value *val);
    const RDNode *getNode(const llvm::Value *val) const;

    // let the user get the nodes map, so that we can
    // map the points-to informatio back to LLVM nodes
    const std::unordered_map<const llvm::Value *, RDNode *>& getNodesMap() const;
    const std::unordered_map<const llvm::Value *, RDNode *>& getMapping() const;

    RDNode *getMapping(const llvm::Value *val);
    const RDNode *getMapping(const llvm::Value *val) const;

    bool isUse(const llvm::Value *val) const {
        auto nd = getNode(val);
        return nd && !nd->getUses().empty();
    }

    bool isDef(const llvm::Value *val) const {
        auto nd = getNode(val);
        return nd && (!nd->getDefines().empty() || !nd->getOverwrites().empty());
    }

    std::vector<RDNode *> getNodes() {
        assert(RDA);
        // FIXME: this is insane, we should have this method defined here
        // not in RDA
        return RDA->getNodes(getRoot());
    }

    /*
    std::vector<RDNode *> getReachingDefinitions(RDNode *where, RDNode *mem,
                                                 const Offset& off, const Offset& len) {
        return RDA->getReachingDefinitions(where, n, off, len, ret);
    }
    */

    inline void getReachingDefinitions(RDNode *use, RDNodeSetVector &result) {
      RDA->getReachingDefinitions(use, result);
    }

    inline void getReachingDefinitions(llvm::Value *use,
                                       RDNodeSetVector &result) {
      auto node = getNode(use);
      assert(node);
      getReachingDefinitions(node, result);
    }

    // return instructions that define the given value
    // (the value must read from memory, e.g. LoadInst)
    void getLLVMReachingDefinitions(llvm::Value *use,
                                    std::vector<llvm::Value *> &defs);
};


} // namespace rd
} // namespace dg
} // namespace analysis

#endif
