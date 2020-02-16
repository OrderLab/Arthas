#include <iostream>

#include <llvm/Support/CommandLine.h>

#include "Matcher/Matcher.h"
#include "Utils/LLVM.h"
#include "Utils/String.h"

using namespace std;
using namespace llvm;

cl::opt<string> inputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);
cl::opt<string> criteria(cl::Positional, cl::desc("<criteria>"), cl::Required);

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv);

  LLVMContext context;
  unique_ptr<Module> M = parseModule(context, inputFilename);
  if (!M) {
    errs() << "Failed to parse '" << inputFilename << "' file:\n";
    return 1;
  }
  errs() << "Successfully parsed " << inputFilename << "\n";
  vector<string> criteriaList;
  splitList(criteria, ',', criteriaList);
  Matcher matcher;
  matcher.process(*M);
  if (criteriaList.size() == 1) {
    vector<string> parts;
    splitList(criteria, ':', parts);
    if (parts.size() < 2) {
      errs() << "Invalid criterion '" << criteria << "', must be file:line_number format\n";
      return 1;
    }
    FileLine fl(parts[0], atoi(parts[1].c_str()));
    MatchResult result;
    matcher.matchInstrsCriterion(fl, &result);
    errs() << result << "\n";
  } else {
    vector<FileLine> fileLines;
    for (string criterion : criteriaList) {
      vector<string> parts;
      splitList(criterion, ':', parts);
      if (parts.size() < 2) {
        errs() << "Invalid criterion '" << criterion << "', must be file:line_number format\n";
        return 1;
      }
      FileLine fl(parts[0], atoi(parts[1].c_str()));
      fileLines.push_back(fl);
    }
    vector<MatchResult> results(fileLines.size());
    matcher.matchInstrsCriteria(fileLines, results);
    for (MatchResult& result : results) {
      errs() << result << "\n";
    }
  }
}
