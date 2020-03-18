#ifndef _DG_TOOL_LLVM_SLICER_H_
#define _DG_TOOL_LLVM_SLICER_H_

#include <ctime>

// ignore unused parameters in LLVM libraries
#if (__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#if (__clang__)
#pragma clang diagnostic pop // ignore -Wunused-parameter
#else
#pragma GCC diagnostic pop
#endif

#include "dg/llvm/LLVMDependenceGraph.h"
#include "dg/llvm/LLVMDependenceGraphBuilder.h"
#include "dg/llvm/LLVMSlicer.h"

#include "dg/llvm/LLVMDGAssemblyAnnotationWriter.h"
#include "dg/util/TimeMeasure.h"

#include "llvm-slicer-opts.h"

/// --------------------------------------------------------------------
//   - Slicer class -
//
//  The main class that takes the bitcode, constructs the dependence graph
//  and then slices it w.r.t given slicing criteria.
//  The usual workflow is as follows:
//
//  Slicer slicer(M, options);
//  slicer.buildDG();
//  slicer.mark(criteria);
//  slicer.slice();
//
//  In the case that the slicer is not used for slicing,
//  but just for building the graph, the user may do the following:
//
//  Slicer slicer(M, options);
//  slicer.buildDG();
//  slicer.computeDependencies();
//
//  or:
//
//  Slicer slicer(M, options);
//  slicer.buildDG(true /* compute dependencies */);
//
/// --------------------------------------------------------------------
class Slicer {
    llvm::Module *M{};
    const SlicerOptions& _options;

    dg::llvmdg::LLVMDependenceGraphBuilder _builder;
    std::unique_ptr<dg::LLVMDependenceGraph> _dg{};

    dg::LLVMSlicer slicer;
    uint32_t slice_id = 0;
    bool _computed_deps{false};

public:
    Slicer(llvm::Module *mod, const SlicerOptions& opts)
    : M(mod), _options(opts),
      _builder(mod, _options.dgOptions) { assert(mod && "Need module"); }

    const dg::LLVMDependenceGraph& getDG() const { return *_dg.get(); }
    dg::LLVMDependenceGraph& getDG() { return *_dg.get(); }

    // Mirror LLVM to nodes of dependence graph,
    // No dependence edges are added here unless the
    // 'compute_deps' parameter is set to true.
    // Otherwise, dependencies must be computed later
    // using computeDependencies().
    bool buildDG(bool compute_deps = false) {
        _dg = std::move(_builder.constructCFGOnly());

        if (!_dg) {
            llvm::errs() << "Building the dependence graph failed!\n";
            return false;
        }

        if (compute_deps)
            computeDependencies();

        return true;
    }

    // Explicitely compute dependencies after building the graph.
    // This method can be used to compute dependencies without
    // calling mark() afterwards (mark() calls this function).
    // It must not be called before calling mark() in the future.
    void computeDependencies() {
        assert(!_computed_deps && "Already called computeDependencies()");
        // must call buildDG() before this function
        assert(_dg && "Must build dg before computing dependencies");

        _dg = _builder.computeDependencies(std::move(_dg));
        _computed_deps = true;

        const auto& stats = _builder.getStatistics();
        llvm::errs() << "[llvm-slicer] CPU time of pointer analysis: " << double(stats.ptaTime) / CLOCKS_PER_SEC << " s\n";
        llvm::errs() << "[llvm-slicer] CPU time of reaching definitions analysis: " << double(stats.rdaTime) / CLOCKS_PER_SEC << " s\n";
        llvm::errs() << "[llvm-slicer] CPU time of control dependence analysis: " << double(stats.cdTime) / CLOCKS_PER_SEC << " s\n";
    }

    // Mark the nodes from the slice.
    // This method calls computeDependencies(),
    // but buildDG() must be called before.
    bool mark(std::set<dg::LLVMNode *>& criteria_nodes)
    {
        assert(_dg && "mark() called without the dependence graph built");
        assert(!criteria_nodes.empty() && "Do not have slicing criteria");

        dg::debug::TimeMeasure tm;

        // compute dependece edges
        computeDependencies();

        // unmark this set of nodes after marking the relevant ones.
        // Used to mimic the Weissers algorithm
        std::set<dg::LLVMNode *> unmark;

        if (_options.removeSlicingCriteria)
            unmark = criteria_nodes;

        _dg->getCallSites(_options.additionalSlicingCriteria, &criteria_nodes);

        for (auto& funcName : _options.preservedFunctions)
            slicer.keepFunctionUntouched(funcName.c_str());

        slice_id = 0xdead;

        tm.start();
        for (dg::LLVMNode *start : criteria_nodes)
          slice_id = slicer.mark(start, slice_id, _options.forwardSlicing);

        assert(slice_id != 0 && "Somethig went wrong when marking nodes");

        // if we have some nodes in the unmark set, unmark them
        for (dg::LLVMNode *nd : unmark)
            nd->setSlice(0);

        tm.stop();
        tm.report("[llvm-slicer] Finding dependent nodes took");

        return true;
    }

    bool slice()
    {
        assert(_dg && "Must run buildDG() and computeDependencies()");
        assert(slice_id != 0 && "Must run mark() method before slice()");

        dg::debug::TimeMeasure tm;

        tm.start();
        slicer.slice(_dg.get(), nullptr, slice_id);

        tm.stop();
        tm.report("[llvm-slicer] Slicing dependence graph took");

        dg::analysis::SlicerStatistics& st = slicer.getStatistics();
        llvm::errs() << "[llvm-slicer] Sliced away " << st.nodesRemoved
                     << " from " << st.nodesTotal << " nodes in DG\n";

        return true;
    }

    ///
    // Create new empty main in the module. If 'call_entry' is set to true,
    // then call the entry function from the new main (if entry is not main),
    // otherwise the main is going to be empty
    bool createEmptyMain(bool call_entry = false)
    {
        llvm::LLVMContext& ctx = M->getContext();
        llvm::Function *main_func = M->getFunction("main");
        if (!main_func) {
            auto C = M->getOrInsertFunction("main",
                                            llvm::Type::getInt32Ty(ctx)
#if LLVM_VERSION_MAJOR < 5
                                            , nullptr
#endif // LLVM < 5
                                            );
#if LLVM_VERSION_MAJOR < 9
            if (!C) {
                llvm::errs() << "Could not create new main function\n";
                return false;
            }

            main_func = llvm::cast<llvm::Function>(C);
#else
            main_func = llvm::cast<llvm::Function>(C.getCallee());
#endif
        } else {
            // delete old function body
            main_func->deleteBody();
        }

        assert(main_func && "Do not have the main func");
        assert(main_func->size() == 0 && "The main func is not empty");

        // create new function body
        llvm::BasicBlock* blk = llvm::BasicBlock::Create(ctx, "entry", main_func);

        if (call_entry && _options.dgOptions.entryFunctionName != "main") {
          llvm::Function* entry =
              M->getFunction(_options.dgOptions.entryFunctionName);
          assert(entry && "The entry function is not present in the module");

          // TODO: we should set the arguments to undef
          llvm::CallInst::Create(entry, "entry", blk);
        }

        llvm::Type *Ty = main_func->getReturnType();
        llvm::Value *retval = nullptr;
        if (Ty->isIntegerTy())
            retval = llvm::ConstantInt::get(Ty, 0);
        llvm::ReturnInst::Create(ctx, retval, blk);

        return true;
    }
};

#endif // _DG_TOOL_LLVM_SLICER_H_
