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

class ScopeInfoFinder {
  public:
   static unsigned getInstLine(const llvm::Instruction *);
   static unsigned getFirstLine(llvm::Function *);
   static unsigned getLastLine(llvm::Function *);
   static bool getBlockScope(Scope &, llvm::BasicBlock *);
};

class Matcher {
 public:
  typedef llvm::SmallVectorImpl<llvm::DISubprogram *>::const_iterator sp_iterator;
  typedef llvm::SmallVectorImpl<llvm::DICompileUnit *>::const_iterator cu_iterator;

 protected:
  bool initialized;
  bool processed;
  std::string filename;
  const char *patchname;
  llvm::DebugInfoFinder Finder;

 public:
  llvm::SmallVector<llvm::DISubprogram *, 8> MySPs;
  llvm::SmallVector<llvm::DICompileUnit *, 8> MyCUs;

  int patchstrips;
  int debugstrips;

 public:
  Matcher(int d_strips = 0, int p_strips = 0) {
    patchstrips = p_strips;
    debugstrips = d_strips;
    initialized = false;
    processed = false;
  }

  llvm::Instruction *matchInstruction(llvm::inst_iterator &, llvm::Function *, Scope &);

  void process(llvm::Module &M) 
  {
    processCompileUnits(M);
    processSubprograms(M); 
    processed = true;
  }

  void setstrips(int p_strips, int d_strips) 
  {
    patchstrips = p_strips; 
    debugstrips = d_strips; 
  }

  inline sp_iterator sp_begin() { return MySPs.begin(); }
  inline sp_iterator sp_end() { return MySPs.end(); }

  inline cu_iterator cu_begin() { return MyCUs.begin(); }
  inline cu_iterator cu_end() { return MyCUs.end(); }

 protected:
  cu_iterator matchCompileUnit(llvm::StringRef);
  sp_iterator slideToFile(llvm::StringRef);
  void processCompileUnits(llvm::Module &);
  void processSubprograms(llvm::Module &);
  void processInst(llvm::Function *);
  void processBasicBlock(llvm::Function *);

  bool initName(llvm::StringRef);
  void dumpSPs();
};

#endif
