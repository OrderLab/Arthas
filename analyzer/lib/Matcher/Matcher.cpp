//===---- Matcher.h - Match Scope To IR Elements----*- C++ -*-===//
//
// Author: Ryan Huang <ryanhuang@cs.ucsd.edu>
//
//===----------------------------------------------------------------------===//
//

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Matcher/Matcher.h"
#include "Utils/Path.h"
#include "Utils/String.h"

static bool DEBUG_MATCHER = false;

using namespace std;
using namespace llvm;
using namespace llvm::matching;

bool cmpDICU(DICompileUnit *CU1, DICompileUnit *CU2) {
  int cmp = CU1->getDirectory().compare(CU2->getDirectory());
  if (cmp == 0)
    cmp = CU1->getFilename().compare(CU2->getFilename());
  return cmp >= 0 ? false : true;
}

bool cmpDISP(DISubprogram *SP1, DISubprogram *SP2) {
  int cmp = SP1->getDirectory().compare(SP2->getDirectory());
  if (cmp == 0) {
    cmp = SP1->getFilename().compare(SP2->getFilename());
    if (cmp == 0) {
      cmp = SP1->getLine() - SP2->getLine();
    }
  }
  return cmp >= 0 ? false : true;
}

inline bool skipFunction(Function *F)
{
  // Skip intrinsic functions and function declaration because DT only 
  // works with function definition.
  return F->isDeclaration() || F->getName().startswith("llvm.dbg");
}

llvm::raw_ostream & operator<<(llvm::raw_ostream& os, const MatchResult& result)
{
  if (result.matched) {
    DISubprogram *SP = result.func->getSubprogram();
    unsigned start_line = ScopeInfoFinder::getFirstLine(result.func);
    unsigned end_line = ScopeInfoFinder::getLastLine(result.func);
    os << "Matched function <" << getFunctionName(SP) << ">()";
    os << "@" << SP->getDirectory() << "/" << SP->getFilename();
    os << ":" << start_line << "," << end_line << "\n";
    for (Instruction *inst : result.instrs) {
      os << "- matched instruction: " << *inst << "\n";
    }
  } else {
    os << "No match found";
  }
  return os;
}

unsigned ScopeInfoFinder::getInstLine(const Instruction *I) {
  auto Loc = I->getDebugLoc();
  if (!Loc) {
    if (DEBUG_MATCHER) {
      errs() << "Unknown LOC" << "\n";
    }
    return 0;
  }
  return Loc.getLine();
}

unsigned ScopeInfoFinder::getFirstLine(Function *F)
{
  DISubprogram *SP = F->getSubprogram();
  if (SP == NULL)
    return 0;
  // line number for start of the function
  return SP->getLine();
}

unsigned ScopeInfoFinder::getLastLine(Function *F)
{
  if (F == NULL || F->begin() == F->end()) //empty block
    return 0;
  const BasicBlock & BB = F->back();
  const Instruction & I = BB.back();
  auto Loc = I.getDebugLoc();
  if (!Loc) return 0;
  return Loc.getLine();
}

bool ScopeInfoFinder::getBlockScope(Scope & scope, BasicBlock *B)
{
  if (B->begin() == B->end()) // empty block
    return false;

  /** Use first and last instruction to get the scope information **/
  Instruction *first = & B->front();
  Instruction *last = & B->back();
  if (first == NULL || last == NULL) {
    errs() << "NULL scope instructions " << "\n";
    return false;
  }
  unsigned b = getInstLine(first);
  unsigned e = getInstLine(last);
  if (b == 0 || e == 0) {
    return false;
  }
  scope.begin = b;
  scope.end = e;
  return true;
}

bool FileLine::fromCriterionStr(string criterion, FileLine &result)
{
  vector<string> parts;
  splitList(criterion, ':', parts);
  if (parts.size() < 2) {
    return false;
  }
  result.file = parts[0];
  result.line = atoi(parts[1].c_str());
  return true;
}

bool FileLine::fromCriteriaStr(string criteria, vector<FileLine> &results) {
  vector<string> criteriaList;
  splitList(criteria, ',', criteriaList);
  for (string criterion : criteriaList) {
    vector<string> parts;
    splitList(criterion, ':', parts);
    if (parts.size() < 2) {
      return false;
    }
    results.push_back(FileLine(parts[0], atoi(parts[1].c_str())));
  }
  return true;
}

void Matcher::process(Module &M)
{
  // With the new LLVM version, it seems we can no longer retrieve the
  // corresponding Function * from a DISubprogram. Therefore, it is no
  // longer useful to use the DebugInfoFinder...
  // finder.processModule(M);
  module = &M;
  processed = true;
}

StringRef getFunctionName(const DISubprogram *SP) {
  if (!SP->getLinkageName().empty()) return SP->getLinkageName();
  return SP->getName();
}

void Matcher::dumpSP(DISubprogram *SP) {
  errs() << "@" << SP->getDirectory() << "/" << SP->getFilename();
  errs() << ":" << SP->getLine() << " <" << getFunctionName(SP);
  errs() << ">() \n";
}

string Matcher::normalizePath(StringRef fname) {
  char *canon = canonpath(fname.data(), NULL);
  string filename;
  if (canon == NULL) {
    errs() << "Warning: patchname is NULL\n";
    return filename;
  }
  filename.assign(canon);
  free(canon);
  if (strips > 0) {
    const char *normalized = stripname(filename.c_str(), strips);
    if (strlen(normalized) == 0) {
      errs() << "Warning: filename is empty after strip\n";
    }
    return string(normalized);
  } else {
    return filename;
  }
}

bool Matcher::spMatchFilename(DISubprogram *sp, const char *filename)
{
    std::string debugname;
    // Filename may already contains the path information
    if (sp->getFilename().size() > 0 && sp->getFilename()[0] == '/')
      debugname = sp->getFilename();
    else
      debugname = sp->getDirectory().str() + "/" + sp->getFilename().str();
    return pathendswith(debugname.c_str(), filename);
}

bool Matcher::matchInstrsInFunction(unsigned int line, Function *func, MatchInstrs &result)
{
  unsigned l = 0;
  Instruction * inst = NULL;
  bool matched = false;
  for (inst_iterator ii = inst_begin(func), ie = inst_end(func); 
      ii != ie; ++ii) {
    inst = &*ii;
    l = ScopeInfoFinder::getInstLine(inst);
    if (l == line) {
      matched = true;
      result.push_back(inst);
    } else if (l > line) {
      break;
    }
  }
  return matched;
}

bool Matcher::matchInstrsCriteria(vector<FileLine> &criteria, 
    vector<MatchResult> &results) {
  results.resize(criteria.size());
  size_t sz = criteria.size();
  for (auto &F : module->functions()) {
    if (skipFunction(&F))
      continue;
    DISubprogram *SP = F.getSubprogram();
    for (size_t i = 0; i < sz; ++i) {
      // if a criterion has been matched before, try the next one
      if (results[i].matched) continue;
      if (spMatchFilename(SP, criteria[i].file.c_str())) {
        unsigned start_line = SP->getLine();
        unsigned end_line = ScopeInfoFinder::getLastLine(&F);
        unsigned line = criteria[i].line;
        if (line >= start_line && line <= end_line) {
          results[i].matched =
              matchInstrsInFunction(line, &F, results[i].instrs);
          results[i].func = &F;
        }
      }
    }
    bool allMatched = true;
    for (size_t i = 0; i < sz; ++i)
      if (!results[i].matched)
        allMatched = false;
    // if all criteria has been matched, there is no point continuing
    // iterating the remaining functions.
    if (allMatched)
      return true;
  }
  return true;
}

bool Matcher::matchInstrsCriterion(FileLine criterion, MatchResult *result) {
  for (auto &F : module->functions()) {
    if (skipFunction(&F))
      continue;
    DISubprogram *SP = F.getSubprogram();
    if (spMatchFilename(SP, criterion.file.c_str())) {
      if (DEBUG_MATCHER) {
        dumpSP(SP);
      }
      unsigned start_line = SP->getLine();
      unsigned end_line = ScopeInfoFinder::getLastLine(&F);
      if (criterion.line >= start_line && criterion.line <= end_line) {
        result->matched = matchInstrsInFunction(criterion.line, &F, result->instrs);
        result->func = &F;
        return true;
      }
    }
  }
  return false;
}
