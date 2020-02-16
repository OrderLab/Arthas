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

bool skipFunction(Function *F)
{
  // Skip intrinsic functions and function declaration because DT only 
  // works with function definition.
  if (F == NULL || F->getName().startswith("llvm.dbg") || 
      F->isDeclaration()) 
    return true;
  return false;
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

void Matcher::process(Module &M)
{
  finder.processModule(M);
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

bool Matcher::matchInstructions(std::string criteria, MatchResult *result) {
  vector<string> parts;
  splitList(criteria, ':', parts);
  if (parts.size() < 2)
    return false;
  string file = parts[0];
  unsigned int line = atoi(parts[1].c_str());
  Function *func = nullptr;
  // for (DISubprogram *SP : finder.subprograms()) {
  for (auto &F : module->functions()) {
    if (F.isDeclaration()) continue;
    DISubprogram *SP = F.getSubprogram();
    std::string debugname;
    // Filename may already contains the path information
    if (SP->getFilename().size() > 0 && SP->getFilename()[0] == '/')
      debugname = SP->getFilename();
    else
      debugname = SP->getDirectory().str() + "/" + SP->getFilename().str();
    if (pathendswith(debugname.c_str(), file.c_str())) {
      if (DEBUG_MATCHER) {
        dumpSP(SP);
      }
      if (SP->getLine() > line) {
        // if the beginning line of this function is larger than the target line
        // we have iterated past the target function
        break;
      } else {
        func = &F;
      }
    }
  }
  if (func != nullptr) {
    unsigned l = 0;
    Instruction * inst = NULL;
    for (inst_iterator ii = inst_begin(func), ie = inst_end(func); ii != ie; ++ii) {
      inst = &*ii;
      l = ScopeInfoFinder::getInstLine(inst);
      if (l == line) {
        result->instrs.push_back(inst);
      } else if (l > line) {
        break;
      }
    }
    result->func = func;
    result->matched = true;
  }
  return true;
}
