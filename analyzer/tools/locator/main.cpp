// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <iostream>

#include <llvm/Support/CommandLine.h>

#include "Matcher/Matcher.h"
#include "Utils/LLVM.h"
#include "Utils/String.h"

using namespace std;
using namespace llvm;
using namespace llvm::matching;

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
    FileLine fl;
    if (!FileLine::fromCriterionStr(criteria, fl)) {
      errs() << "Invalid criterion '" << criteria << "', must be file:line_number format\n";
      return 1;
    }
    MatchResult result;
    matcher.matchInstrsCriterion(fl, &result);
    errs() << result << "\n";
  } else {
    vector<FileLine> fileLines;
    if (!FileLine::fromCriteriaStr(criteria, fileLines)) {
      errs() << "Invalid criteria '" << criteria << "', must be file:line_number format\n";
      return 1;
    }
    vector<MatchResult> results(fileLines.size());
    matcher.matchInstrsCriteria(fileLines, results);
    for (MatchResult& result : results) {
      errs() << result << "\n";
    }
  }
}
