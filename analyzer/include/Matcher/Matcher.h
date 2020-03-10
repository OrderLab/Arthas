//===---- Matcher.h - Match Scope To IR Elements----*- C++ -*-===//
//
// Author: Ryan Huang <ryanhuang@cs.ucsd.edu>
//
//===----------------------------------------------------------------------===//
#ifndef __MATCHER_H_
#define __MATCHER_H_

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

namespace llvm {
namespace matching {

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

class ScopeInfoFinder {
  public:
   static unsigned getInstLine(const llvm::Instruction *);
   static unsigned getFirstLine(llvm::Function *);
   static unsigned getLastLine(llvm::Function *);
   static bool getBlockScope(Scope &, llvm::BasicBlock *);
};

class FileLine {
  public:
   std::string file;
   unsigned int line;

   FileLine(std::string f, unsigned int l) : file(f), line(l) {}
   FileLine(){}

   static bool fromCriterionStr(std::string criterion, FileLine &result);
   static bool fromCriteriaStr(std::string criteria, std::vector<FileLine> &results);
};

typedef llvm::SmallVector<llvm::Instruction *, 8> MatchInstrs;

class MatchResult {
  public:
    MatchResult()
    {
      matched = false;
    }

    void print(raw_ostream &os) const;

  public:
   bool matched;
   MatchInstrs instrs;
   llvm::Function *func;
};

class Matcher {
 protected:
  bool initialized;
  bool _processed;
  int strips;

  llvm::DebugInfoFinder finder;
  llvm::Module *module;

 public:
  Matcher(int path_strips = 0) {
    strips = path_strips;
    initialized = false;
    _processed = false;
  }

  bool matchInstrsCriterion(FileLine criterion, MatchResult *result);
  bool matchInstrsCriteria(std::vector<FileLine> &criteria, std::vector<MatchResult> &results);

  bool processed() const { return _processed; }
  void process(llvm::Module &M);

  void setStrip(int path_strips) { strips = path_strips; }

  void dumpSP(llvm::DISubprogram *SP);
  std::string normalizePath(llvm::StringRef fname);

 protected:
  bool spMatchFilename(llvm::DISubprogram *sp, const char *filename);
  bool matchInstrsInFunction(unsigned int line, llvm::Function *func, MatchInstrs &result);
};

} // namespace matching

inline raw_ostream &operator<<(raw_ostream &os, const matching::MatchResult &result)
{
  result.print(os);
  return os;
}

} // namespace llvm

bool cmpDISP(llvm::DISubprogram *, llvm::DISubprogram *);
bool cmpDICU(llvm::DICompileUnit *, llvm::DICompileUnit *);
bool skipFunction(llvm::Function *);
llvm::StringRef getFunctionName(const llvm::DISubprogram *SP);

#endif /* __MATCHER_H_ */
