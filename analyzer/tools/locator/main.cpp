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

  Matcher matcher;
  matcher.process(*M);
  MatchResult result;
  matcher.matchInstructions("test/loop1.c:24", &result);
  if (result.matched) {
    DISubprogram *SP = result.func->getSubprogram();
    unsigned start_line = ScopeInfoFinder::getFirstLine(result.func);
    unsigned end_line = ScopeInfoFinder::getLastLine(result.func);
    errs() << "Matched function <" << getFunctionName(SP) << ">()";
    errs() << "@" << SP->getDirectory() << "/" << SP->getFilename();
    errs() << ":" << start_line << "," << end_line << "\n";
    for (Instruction *inst : result.instrs) {
      errs() << "- matched instruction: " << *inst << "\n";
    }
  }
}
