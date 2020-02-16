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
  if (F == NULL || F->begin() == F->end()) //empty block
    return 0;
  const BasicBlock & BB = F->front();
  const Instruction & I = BB.front();
  auto Loc = I.getDebugLoc();
  if (!Loc) return 0;
  return Loc.getLine();
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
  if (mydebuginfo) {
    processCompileUnits(M);
    processSubprograms(M); 
  } else {
    finder.processModule(M);
  }
  module = &M;
  processed = true;
}

void Matcher::processInst(Function *F) {
  for (Function::iterator FI = F->begin(), FE = F->end(); FI != FE; FI++) {
    /** Get each instruction's scope information **/
    for (BasicBlock::iterator BI = FI->begin(), BE = FI->end(); BI != BE; BI++) {
      auto Loc = BI->getDebugLoc();
      if (!Loc) continue;
      errs() << "Block :" << Loc->getLine() << ", " << Loc->getColumn() << "\n";
    }
  }
}

void Matcher::processBasicBlock(Function *F)
{
  for (Function::iterator FI = F->begin(), FE = F->end(); FI != FE; FI++) {
    /** Use first and last instruction to get the scope information **/
    Instruction *first = & FI->front();
    Instruction *last = & FI->back();
    if (first == NULL || last == NULL) {
      errs() << "NULL scope instructions " << "\n";
      continue;
    }
    auto Loc = first->getDebugLoc();
    if (!Loc) {
      errs() << "Unknown LOC information" << "\n";
      continue;
    }
    errs() << "Block :" << Loc.getLine();
    Loc = last->getDebugLoc();
    if (!Loc) {
      errs() << "Unknown LOC information" << "\n";
      continue;
    }
    errs() << ", " << Loc.getLine() << "\n";
  }
}

void Matcher::processCompileUnits(Module &M)
{
  MyCUs.clear();

  for (auto *CU : M.debug_compile_units()) {
    MyCUs.push_back(CU);
  }

  /** Sort based on file name, directory and line number **/
  std::sort(MyCUs.begin(), MyCUs.end(), cmpDICU);
  if (DEBUG_MATCHER) {
    cu_iterator I, E;
    for (I = MyCUs.begin(), E = MyCUs.end(); I != E; I++) {
      errs() << "CU: " << (*I)->getDirectory() << "/" << (*I)->getFilename() << "\n";
    }
  }
}

static StringRef getFunctionName(const DISubprogram *SP) {
  if (!SP->getLinkageName().empty())
    return SP->getLinkageName();
  return SP->getName();
}

void Matcher::dumpSP(DISubprogram *SP) {
  errs() << "@" << SP->getDirectory() << "/" << SP->getFilename();
  errs() << ":" << SP->getLine() << " <" << getFunctionName(SP);
  errs() << ">() \n";
}

void Matcher::dumpSPs() {
  sp_iterator I, E;
  for (I = MySPs.begin(), E = MySPs.end(); I != E; I++) {
    dumpSP(*I);
  }
}

void Matcher::processSubprograms(Module &M)
{
  /** DIY SP finder **/
  MySPs.clear();
  errs() << "MySPs\n";
  for (auto *CU : M.debug_compile_units()) {
    // errs() << "CU: " << CU->getDirectory() << "/" << CU->getFilename() << "\n";
    errs() << CU->getFilename() << "\n";
    for (auto *RT : CU->getRetainedTypes())
      if (auto *SP = dyn_cast<DISubprogram>(RT)) {
        MySPs.push_back(SP);
        if (DEBUG_MATCHER)
          errs() << "DIS: " << SP->getName() << ", " << SP->getDisplayName() << "\n";
      }
    for (auto *Import : CU->getImportedEntities()) {
      auto *Entity = Import->getEntity().resolve();
      if (auto *SP = dyn_cast<DISubprogram>(Entity)) {
        MySPs.push_back(SP);
        if (DEBUG_MATCHER)
          errs() << "DIS: " << SP->getName() << ", " << SP->getDisplayName() << "\n";
      }
    }
  }

  /** Sort based on file name, directory and line number **/
  std::sort(MySPs.begin(), MySPs.end(), cmpDISP);
  if (DEBUG_MATCHER) dumpSPs();
}

Matcher::cu_iterator Matcher::matchCompileUnit(StringRef fullname)
{
  if (!initName(fullname)) return MyCUs.end();

  initialized = true;

  /* TODO use binary search here */

  cu_iterator I = cu_begin(), E = cu_end();

  while(I != E) {
    std::string debugname;
    DICompileUnit *CU = *I;
    // Filename may already contains the path information
    if (CU->getFilename().size() > 0 && CU->getFilename()[0] == '/')
      debugname = CU->getFilename();
    else
      debugname = CU->getDirectory().str() + "/" + CU->getFilename().str();
    if (pathendswith(debugname.c_str(), patchname)) break;
    I++;
  }
  if (I == E)
    errs() << "Warning: no matching file(" << patchname << ") was found in the CUs\n";
  return I;
}

bool Matcher::initName(StringRef fname) {
  char *canon = canonpath(fname.data(), NULL);  
  if (canon == NULL) {
    errs() << "Warning: patchname is NULL\n";
    return false;
  }
  filename.assign(canon);
  free(canon);
  patchname = stripname(filename.c_str(), patchstrips);
  if (strlen(patchname) == 0) {
    errs() << "Warning: patchname is empty after strip\n";
    return false;
  }
  return true;
}

/* *
 * Adjust sp_iterator to the starting position of
 * the target source file region.
 *
 * Assumption: MySPs contains the sorted subprograms
 * */
Matcher::sp_iterator Matcher::slideToFile(StringRef fname)
{
  if (!processed) {
    errs() << "Warning: Matcher hasn't processed module\n";
    return sp_end();
  }
  sp_iterator I = sp_begin(), E = sp_end();

  while(I != E) {
    std::string debugname;
    DISubprogram * SP = *I;
    if (SP->getFilename().size() > 0 && SP->getFilename()[0] == '/')
      debugname = SP->getFilename();
    else
      debugname = SP->getDirectory().str() + "/" + SP->getFilename().str();
    if (pathendswith(debugname.c_str(), patchname)) {
      break;
    }
    I++;
  }
  if (I == E)
    errs() << "Warning: no matching file(" << patchname << ") was found in the CU\n";
  return I;
}

bool Matcher::matchFileLine(std::string criteria, MatchResult *result)
{
  vector<string> parts;
  splitList(criteria, ':', parts);
  if (parts.size() < 2)
    return false;
  string file = parts[0];
  unsigned int line = atoi(parts[1].c_str());
  if (mydebuginfo) {
    return false;
  } else {
    // for (DISubprogram *SP : finder.subprograms()) {
    Function *func = nullptr;
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
        dumpSP(SP);
        if (SP->getLine() > line) {
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
}
