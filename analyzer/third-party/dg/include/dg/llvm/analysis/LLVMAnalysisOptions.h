#ifndef _DG_LLVM_ANALYSIS_OPTIONS_H_
#define _DG_LLVM_ANALYSIS_OPTIONS_H_

#include <string>
#include "dg/analysis/AnalysisOptions.h"

namespace dg {
namespace analysis {

struct LLVMAnalysisOptions {
  // set either the entry function name
  std::string entryFunctionName{"main"};
  // or directly the entry function
  llvm::Function* entryFunction{nullptr};

  // is the analysis intraprocedural (that is, only of the
  // entry function?)
  bool intraProcedural{false};

  LLVMAnalysisOptions& setEntryFunctionName(const std::string& e) {
    entryFunctionName = e;
    return *this;
  }

  LLVMAnalysisOptions& setEntryFunction(llvm::Function* e) {
    entryFunction = e;
    return *this;
  }

  LLVMAnalysisOptions& setIntraprocedural(bool b) {
    intraProcedural = b;
    return *this;
  }
};

}  // namespace analysis
}  // namespace dg

#endif // _DG_LLVM_ANALYSIS_OPTIONS_H_
