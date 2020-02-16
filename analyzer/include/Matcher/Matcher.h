//===---- Matcher.h - Match Scope To IR Elements----*- C++ -*-===//
//
// Author: Ryan Huang <ryanhuang@cs.ucsd.edu>
//
//===----------------------------------------------------------------------===//
#ifndef ___MATCHER__H_
#define ___MATCHER__H_

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/ADT/Statistic.h"

#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Dwarf.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>
#include <deque>

#include "Scope.h"

template <class T1, class T2>
struct Pair {
  typedef T1 first_type;
  typedef T2 second_type;

  T1 first;
  T2 second;

  Pair() : first(T1()), second(T2()) {}
  Pair(const T1& x, const T2& y) : first(x), second(y) {}
  template <class U, class V> Pair (const Pair<U,V> &p) : first(p.first), second(p.second) { }
};

typedef Pair<llvm::DISubprogram, int> DISPExt;

bool cmpDISP(llvm::DISubprogram *, llvm::DISubprogram *);
bool cmpDICU(llvm::DICompileUnit *, llvm::DICompileUnit *);
bool skipFunction(llvm::Function *);
llvm::StringRef getFunctionName(const llvm::DISubprogram *SP);

class ScopeInfoFinder {
  public:
   static unsigned getInstLine(const llvm::Instruction *);
   static unsigned getFirstLine(llvm::Function *);
   static unsigned getLastLine(llvm::Function *);
   static bool getBlockScope(Scope &, llvm::BasicBlock *);
};

class MatchResult {
  public:
    MatchResult()
    {
      matched = false;
    }

  public:
   bool matched;
   llvm::SmallVector<llvm::Instruction *, 8> instrs;
   llvm::Function *func;
};

class Matcher {
 protected:
  bool initialized;
  bool processed;
  int strips;

  llvm::DebugInfoFinder finder;
  llvm::Module *module;

 public:
  Matcher(int path_strips = 0) {
    strips = path_strips;
    initialized = false;
    processed = false;
  }

  bool matchInstructions(std::string criteria, MatchResult *result);

  void process(llvm::Module &M);

  void setStrip(int path_strips) { strips = path_strips; }

  void dumpSP(llvm::DISubprogram *SP);
  void dumpSPs();

  std::string normalizePath(llvm::StringRef fname);
};

#endif
