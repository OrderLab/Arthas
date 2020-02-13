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

using namespace llvm;

typedef DomTreeNodeBase<BasicBlock> BBNode;

template <class T1, class T2> struct Pair
{
  typedef T1 first_type;
  typedef T2 second_type;

  T1 first;
  T2 second;

  Pair() : first(T1()), second(T2()) {}
  Pair(const T1& x, const T2& y) : first(x), second(y) {}
  template <class U, class V> Pair (const Pair<U,V> &p) : first(p.first), second(p.second) { }
};

typedef struct MetadataElement {
  unsigned key;
  DIDescriptor value;
} MetadataElement;

typedef struct MetadataNode {
  MetadataNode *parent;
  MetadataNode *left;
  MetadataNode *right;
  MetadataElement Element;
} MetadataNode;

class MetadataTree {
  public:
    MetadataNode *root;
    void insert(MetadataNode *);
    MetadataNode *search(unsigned key);
};

typedef Pair<DISubprogram, int> DISPExt;

class DISPCopy {
  public:
    std::string directory;
    std::string filename;
    std::string name;

    unsigned linenumber;
    unsigned lastline;
    Function *function;

  public:
    DISPCopy(DISubprogram & DISP)
    {
      filename = DISP.getFilename();
      //TODO
      // Only copy directory if filename doesn't contain
      // path information.
      // If source code is compiled in another directory,
      // filename in DU will look like:
      // /path/to/src/file
      // if (!(filename.size() > 0 && filename.at(0) == '/'))
      directory = DISP.getDirectory();
      name = DISP.getName();
      linenumber = DISP.getLine();
      lastline = 0;
      function = DISP.getFunction();
    }
};

bool cmpDISP(const DISubprogram &, const DISubprogram &);
bool cmpDISPCopy(const DISPCopy &, const DISPCopy &);

bool skipFunction(Function *);


class ScopeInfoFinder {
  public:
    static unsigned getInstLine(const Instruction *);
    static unsigned getLastLine(Function *);
    static bool getBlockScope(Scope & , BasicBlock *);


};

class Matcher {
  public:
    typedef std::vector<DISPCopy>::iterator sp_iterator;
    typedef std::vector<DICompileUnit>::iterator cu_iterator;

  protected:
    bool initialized;
    bool processed;
    std::string filename;
    const char *patchname;
    Module & module;
    DebugInfoFinder Finder;

  public:
    std::vector<DISPCopy> MySPs;
    std::vector<DICompileUnit> MyCUs;

    int patchstrips;
    int debugstrips;

  public:
    Matcher(Module &M, int d_strips = 0, int p_strips = 0) : module(M)
    {
      patchstrips = p_strips; 
      debugstrips = d_strips; 
      initialized = false;
      processCompileUnits(M); 
      processed = true;
      Finder.processModule(M); // dummy
    }
    //Matcher() {initialized = false; processed = false; patchstrips = 0; debugstrips = 0; }

    sp_iterator setSourceFile(StringRef);
    Function * matchFunction(sp_iterator &, Scope &, bool &);
    Instruction * matchInstruction(inst_iterator &, Function *, Scope &);

    // use off-the-shelf DebugInfoFinder to find the MDNode of a function
    MDNode *getFunctionMD(const Function *F); 


    void process(Module &M) 
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
    cu_iterator matchCompileUnit(StringRef);
    sp_iterator slideToFile(StringRef);
    void processCompileUnits(Module &);
    void processSubprograms(Module &);
    void processSubprograms(DICompileUnit &);
    void processInst(Function *);
    void processBasicBlock(Function *);

    bool initName(StringRef);
    void dumpSPs();

};

#endif
