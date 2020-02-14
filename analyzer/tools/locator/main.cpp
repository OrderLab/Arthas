#include <iostream>

#include <llvm/Support/CommandLine.h>

#include "Matcher/Matcher.h"
#include "Utils/LLVM.h"

using namespace std;
using namespace llvm;

cl::opt<string> inputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv);

  LLVMContext context;
  unique_ptr<Module> M = parseModule(context, inputFilename);
  if (!M) {
    errs() << "Failed to parse '" << inputFilename << "' file:\n";
    return 1;
  }
  errs() << "Successfully parsed " << inputFilename << "\n";

  for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I) {
    Function &F = *I;
    if (F.isDeclaration()) continue;
    DISubprogram *SP = F.getSubprogram();
    errs() << "Function " << F.getName() << " starts at " << SP->getFilename()
           << ":" << SP->getLine() << " ends at " << SP->getFilename() << ":"
           << ScopeInfoFinder::getLastLine(&F) << "\n";
  }
}
