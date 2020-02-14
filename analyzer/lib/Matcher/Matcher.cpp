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

static bool DEBUG_MATCHER = false;

bool cmpDICU(const DICompileUnit *CU1, const DICompileUnit *CU2) 
{ 
  int cmp = CU1->getDirectory().compare(CU2->getDirectory());
  if (cmp == 0)
    cmp = CU1->getFilename().compare(CU2->getFilename());
  return cmp >= 0 ? false : true;
}

bool cmpDISP(const DISubprogram * SP1, const DISubprogram * SP2) 
{ 
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

void Matcher::processInst(Function *F)
{
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

void Matcher::dumpSPs() {
  sp_iterator I, E;
  for (I = MySPs.begin(), E = MySPs.end(); I != E; I++) {
    DISubprogram *SP = *I;
    errs() << "@" << SP->getDirectory() << "/" << SP->getFile();
    errs() << ":" << SP->getName();
    errs() << "([" << SP->getLine() << "," << SP->getScopeLine() << "]) \n";
  }
}

void Matcher::processSubprograms(Module &M)
{
  /** DIY SP finder **/
  MySPs.clear();
  for (auto *CU : M.debug_compile_units()) {
    if (DEBUG_MATCHER)
      errs() << "CU: " << CU->getDirectory() << "/" << CU->getFilename() << "\n";
    processSubprograms(CU);
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

Instruction *Matcher::matchInstruction(inst_iterator &ii, Function *f,
                                       Scope &scope) {
  if (scope.begin > scope.end)
    return NULL;
  Instruction * inst = NULL;
  unsigned line = scope.begin;
  unsigned l = 0;
  inst_iterator ie = inst_end(f);
  for (; ii != ie; ++ii) {
    inst = &*ii;
    l = ScopeInfoFinder::getInstLine(inst);
    if (l == 0)
      continue;
    if (l == line) {
      break;
    }
    if (l > line) { // (*)
      // Didn't find any instruction at line but find
      // one within the range.
      // Mod: [.........]
      //         Inst
      if (l <= scope.end)
        break;
      return NULL; // already passed
    }
  }
  if (ii == ie)
    return NULL;
  ++ii; // adjust fi to the next instruction
  //TODO there could be multiple instructions at one line
  
  // Always assume the next instruction shares the same line.
  // If it doesn't, in the next call to matchInstruction,
  // begin will be adjusted to the next line by place (*)
  scope.begin = l; 
  return inst;
}
