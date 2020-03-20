#ifndef _DG_LLVM_DEPENDENCE_GRAPH_BUILDER_H_
#define _DG_LLVM_DEPENDENCE_GRAPH_BUILDER_H_

#include <ctime>  // std::clock
#include <string>

// ignore unused parameters in LLVM libraries
#if (__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

#if (__clang__)
#pragma clang diagnostic pop  // ignore -Wunused-parameter
#else
#pragma GCC diagnostic pop
#endif

#include "dg/llvm/LLVMDependenceGraph.h"
#include "dg/llvm/analysis/PointsTo/LLVMPointerAnalysisOptions.h"
#include "dg/llvm/analysis/ReachingDefinitions/LLVMReachingDefinitionsAnalysisOptions.h"

#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"
#include "dg/llvm/analysis/ReachingDefinitions/ReachingDefinitions.h"

#include "dg/analysis/Offset.h"
#include "dg/analysis/PointsTo/Pointer.h"
#include "dg/analysis/PointsTo/PointerAnalysisFI.h"
#include "dg/analysis/PointsTo/PointerAnalysisFS.h"
#include "dg/analysis/PointsTo/PointerAnalysisFSInv.h"

#include "dg/llvm/analysis/ThreadRegions/ControlFlowGraph.h"

namespace llvm {
class Module;
class Function;
}

namespace dg {
namespace llvmdg {

using analysis::LLVMPointerAnalysisOptions;
using analysis::LLVMReachingDefinitionsAnalysisOptions;

struct LLVMDependenceGraphOptions {
  LLVMPointerAnalysisOptions PTAOptions{};
  LLVMReachingDefinitionsAnalysisOptions RDAOptions{};

  bool terminationSensitive{true};
  CD_ALG cdAlgorithm{CD_ALG::CLASSIC};

  bool intraProcedural{false};

  bool pointerAnalysis{true};

  bool controlDependency{false};

  bool verifyGraph{false};

  bool threads{false};

  // set either the entry function name
  std::string entryFunctionName{"main"};
  // or directly the entry function
  llvm::Function* entryFunction{nullptr};

  void addAllocationFunction(const std::string& name,
                             analysis::AllocationFunction F) {
    PTAOptions.addAllocationFunction(name, F);
    RDAOptions.addAllocationFunction(name, F);
  }
};

class LLVMDependenceGraphBuilder {
  llvm::Module* _M;
  const LLVMDependenceGraphOptions _options;
  std::unique_ptr<LLVMPointerAnalysis> _PTA{};
  std::unique_ptr<LLVMReachingDefinitions> _RD{};
  std::unique_ptr<LLVMDependenceGraph> _dg{};
  std::unique_ptr<ControlFlowGraph> _controlFlowGraph{};
  llvm::Function* _entryFunction{nullptr};

  struct Statistics {
    uint64_t cdTime{0};
    uint64_t ptaTime{0};
    uint64_t rdaTime{0};
    uint64_t inferaTime{0};
    uint64_t joinsTime{0};
    uint64_t critsecTime{0};
  } _statistics;

  std::clock_t _time_start;
  void _timerStart() { _time_start = std::clock(); }
  uint64_t _timerEnd() { return (std::clock() - _time_start); }

  void _runPointerAnalysis() {
    assert(_PTA && "BUG: No PTA");

    _timerStart();

    if (_options.PTAOptions.isFS())
      _PTA->run<analysis::pta::PointerAnalysisFS>();
    else if (_options.PTAOptions.isFI())
      _PTA->run<analysis::pta::PointerAnalysisFI>();
    else if (_options.PTAOptions.isFSInv())
      _PTA->run<analysis::pta::PointerAnalysisFSInv>();
    else {
      assert(0 && "Wrong pointer analysis");
      abort();
    }

    _statistics.ptaTime = _timerEnd();
  }

  void _runReachingDefinitionsAnalysis() {
    assert(_RD && "BUG: No RD");

    _timerStart();

    if (_options.RDAOptions.isDataFlow()) {
      _RD->run<dg::analysis::rd::ReachingDefinitionsAnalysis>();
    } else if (_options.RDAOptions.isSSA()) {
      _RD->run<dg::analysis::rd::SSAReachingDefinitionsAnalysis>();
    } else {
      assert(false && "unknown RDA type");
      abort();
    }

    _statistics.rdaTime = _timerEnd();
  }

  void _runControlDependenceAnalysis() {
    _timerStart();
    _dg->computeControlDependencies(_options.cdAlgorithm,
                                    _options.terminationSensitive);
    _statistics.cdTime = _timerEnd();
  }

  void _runInterferenceDependenceAnalysis() {
    _timerStart();
    _dg->computeInterferenceDependentEdges(_controlFlowGraph.get());
    _statistics.inferaTime = _timerEnd();
  }

  void _runForkJoinAnalysis() {
    _timerStart();
    _dg->computeForkJoinDependencies(_controlFlowGraph.get());
    _statistics.joinsTime = _timerEnd();
  }

  void _runCriticalSectionAnalysis() {
    _timerStart();
    _dg->computeCriticalSections(_controlFlowGraph.get());
    _statistics.critsecTime = _timerEnd();
  }

  bool verify() const { return _dg->verify(); }

 public:
  LLVMDependenceGraphBuilder(llvm::Module* M)
      : LLVMDependenceGraphBuilder(M, {}) {}

  LLVMDependenceGraphBuilder(llvm::Module* M,
                             const LLVMDependenceGraphOptions& opts)
      : _M(M), _options(opts),
        _PTA(new LLVMPointerAnalysis(M, _options.PTAOptions)),
        _RD(new LLVMReachingDefinitions(M, _PTA.get(), _options.RDAOptions)),
        _dg(new LLVMDependenceGraph(opts.threads)),
        _controlFlowGraph(new ControlFlowGraph(_PTA.get())) {
    _entryFunction = _options.entryFunction
                         ? _options.entryFunction
                         : M->getFunction(_options.entryFunctionName);
    assert(_entryFunction && "The entry function not found");
  }

  LLVMPointerAnalysis* getPTA() { return _PTA.get(); }
  LLVMReachingDefinitions* getRDA() { return _RD.get(); }

  const Statistics& getStatistics() const { return _statistics; }

  // construct the whole graph with all edges
  std::unique_ptr<LLVMDependenceGraph>&& build() {
    // compute data dependencies
    if (_options.pointerAnalysis) {
      _runPointerAnalysis();
    }
    _runReachingDefinitionsAnalysis();

    if (_PTA->getForks().empty()) {
      _dg->setThreads(false);
    }

    // build the graph itself (the nodes, but without edges)
    _dg->build(_M, _PTA.get(), _RD.get(), _entryFunction,
               _options.intraProcedural);

    // insert the data dependencies edges
    _dg->addDefUseEdges();

    // compute and fill-in control dependencies
    if (_options.controlDependency) {
      _runControlDependenceAnalysis();
    }

    if (_options.threads) {
      _controlFlowGraph->buildFunction(_entryFunction);
      _runInterferenceDependenceAnalysis();
      _runForkJoinAnalysis();
      _runCriticalSectionAnalysis();
    }

    // verify if the graph is built correctly
    if (_options.verifyGraph && !_dg->verify()) {
      _dg.reset();
      return std::move(_dg);
    }

    return std::move(_dg);
  }

  // Build only the graph with CFG edges.
  // No dependencies between instructions are added.
  // The dependencies must be filled in by calling computeDependencies()
  // later.
  // NOTE: this function still runs pointer analysis as it is needed
  // for sound construction of CFG in the presence of function pointer calls.
  std::unique_ptr<LLVMDependenceGraph>&& constructCFGOnly() {
    // data dependencies
    if (_options.pointerAnalysis) {
      _runPointerAnalysis();
    }

    if (_PTA->getForks().empty()) {
      _dg->setThreads(false);
    }

    // build the graph itself
    _dg->build(_M, _PTA.get(), _RD.get(), _entryFunction,
               _options.intraProcedural);

    if (_options.threads) {
      _controlFlowGraph->buildFunction(_entryFunction);
    }

    // verify if the graph is built correctly
    if (_options.verifyGraph && !_dg->verify()) {
      _dg.reset();
      return std::move(_dg);
    }

    return std::move(_dg);
  }

  // This method serves to finish the graph construction
  // after constructCFGOnly was used to build the graph.
  // This function takes the dg (returned from the constructCFGOnly)
  // and retains its ownership until it computes the edges.
  // Then it returns the ownership back to the caller.
  std::unique_ptr<LLVMDependenceGraph>&& computeDependencies(
      std::unique_ptr<LLVMDependenceGraph>&& dg) {
    // get the ownership
    _dg = std::move(dg);

    // data-dependence edges
    _runReachingDefinitionsAnalysis();
    _dg->addDefUseEdges();

    // fill-in control dependencies
    if (_options.controlDependency) {
      _runControlDependenceAnalysis();
    }

    if (_options.threads) {
      _runInterferenceDependenceAnalysis();
      _runForkJoinAnalysis();
      _runCriticalSectionAnalysis();
    }

    return std::move(_dg);
  }
};

}  // namespace llvmdg
}  // namespace dg

#endif // _DG_LLVM_DEPENDENCE_GRAPH_BUILDER_H_
