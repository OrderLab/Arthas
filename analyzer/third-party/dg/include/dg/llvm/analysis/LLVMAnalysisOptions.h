#ifndef _DG_LLVM_ANALYSIS_OPTIONS_H_
#define _DG_LLVM_ANALYSIS_OPTIONS_H_

#include <string>
#include "dg/analysis/AnalysisOptions.h"

namespace dg {
namespace analysis {

struct LLVMAnalysisOptions {
  // or directly the entry function
  llvm::Function* entryFunction{nullptr};

  // is the analysis only of the entry function?
  bool entryOnly{false};

  // is the analysis intraprocedural
  bool intraProcedural{false};

  LLVMAnalysisOptions& setEntryFunction(llvm::Function* e) {
    entryFunction = e;
    return *this;
  }

  LLVMAnalysisOptions& setEntryOnly(bool b) {
    entryOnly = b;
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
