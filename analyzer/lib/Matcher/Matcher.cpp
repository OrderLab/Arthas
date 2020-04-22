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

#include <regex>

#include "Matcher/Matcher.h"
#include "Utils/Path.h"
#include "Utils/String.h"

static bool DEBUG_MATCHER = false;

// regular expression for the register format in the LLVM instruction: %N
static const std::regex register_regex("\\%\\d+");

using namespace std;
using namespace llvm;
using namespace llvm::matching;

void MatchResult::print(raw_ostream &os) const {
  if (matched) {
    DISubprogram *SP = func->getSubprogram();
    unsigned start_line = ScopeInfoFinder::getFirstLine(func);
    unsigned end_line = ScopeInfoFinder::getLastLine(func);
    os << "Matched function <" << getFunctionName(SP) << ">()";
    os << "@" << SP->getDirectory() << "/" << SP->getFilename();
    os << ":" << start_line << "," << end_line << "\n";
    for (Instruction *inst : instrs) {
      os << "- matched instruction: " << *inst << "\n";
    }
  } else {
    os << "No match found";
  }
}

bool cmpDICU(DICompileUnit *CU1, DICompileUnit *CU2) {
  int cmp = CU1->getDirectory().compare(CU2->getDirectory());
  if (cmp == 0) cmp = CU1->getFilename().compare(CU2->getFilename());
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

bool InstSourceLocComparator::operator()(Instruction *inst1,
                                         Instruction *inst2) const {
  DISubprogram *sp1 = inst1->getFunction()->getSubprogram();
  DISubprogram *sp2 = inst2->getFunction()->getSubprogram();
  int cmp = sp1->getDirectory().compare(sp2->getDirectory());
  if (cmp == 0) {
    cmp = sp1->getFilename().compare(sp2->getFilename());
    if (cmp == 0) {
      auto &loc1 = inst1->getDebugLoc();
      auto &loc2 = inst2->getDebugLoc();
      unsigned line1 = loc1 ? loc1.getLine() : sp1->getLine();
      unsigned line2 = loc2 ? loc2.getLine() : sp2->getLine();
      cmp = line1 - line2;
    }
  }
  if (reverse_order)
    return cmp > 0;
  else
    return cmp < 0;
}

inline bool skipFunction(Function *F) {
  // Skip intrinsic functions and function declaration because DT only
  // works with function definition.
  return F->isDeclaration() || F->getName().startswith("llvm.dbg");
}

unsigned ScopeInfoFinder::getInstLine(const Instruction *I) {
  auto &Loc = I->getDebugLoc();
  if (!Loc) {
    if (DEBUG_MATCHER) {
      errs() << "Unknown LOC"
             << "\n";
    }
    return 0;
  }
  return Loc.getLine();
}

unsigned ScopeInfoFinder::getFirstLine(Function *F) {
  DISubprogram *SP = F->getSubprogram();
  if (SP == NULL) return 0;
  // line number for start of the function
  return SP->getLine();
}

unsigned ScopeInfoFinder::getLastLine(Function *F) {
  if (F == NULL || F->begin() == F->end())  // empty block
    return 0;
  const BasicBlock &BB = F->back();
  const Instruction &I = BB.back();
  auto &Loc = I.getDebugLoc();
  if (!Loc) return 0;
  return Loc.getLine();
}

bool ScopeInfoFinder::getBlockScope(Scope &scope, BasicBlock *B) {
  if (B->begin() == B->end())  // empty block
    return false;

  /** Use first and last instruction to get the scope information **/
  Instruction *first = &B->front();
  Instruction *last = &B->back();
  if (first == NULL || last == NULL) {
    errs() << "NULL scope instructions "
           << "\n";
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

bool FileLine::fromCriterionStr(string criterion, FileLine &result) {
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

void Matcher::process(Module &M) {
  // With the new LLVM version, it seems we can no longer retrieve the
  // corresponding Function * from a DISubprogram. Therefore, it is no
  // longer useful to use the DebugInfoFinder...
  // finder.processModule(M);
  _module = &M;
  _processed = true;
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
  if (_strips > 0) {
    const char *normalized = stripname(filename.c_str(), _strips);
    if (strlen(normalized) == 0) {
      errs() << "Warning: filename is empty after strip\n";
    }
    return string(normalized);
  } else {
    return filename;
  }
}

bool Matcher::spMatchFilename(DISubprogram *sp, const char *filename) {
  std::string debugname;
  // Filename may already contains the path information
  if (sp->getFilename().size() > 0 && sp->getFilename()[0] == '/')
    debugname = sp->getFilename();
  else
    debugname = sp->getDirectory().str() + "/" + sp->getFilename().str();
  return pathendswith(debugname.c_str(), filename);
}

bool Matcher::matchInstrsInFunction(unsigned int line, Function *func,
                                    MatchInstrs &result) {
  unsigned l = 0;
  Instruction *inst = NULL;
  bool matched = false;
  for (inst_iterator ii = inst_begin(func), ie = inst_end(func); ii != ie;
       ++ii) {
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
  for (auto &F : _module->functions()) {
    if (skipFunction(&F)) continue;
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
      if (!results[i].matched) allMatched = false;
    // if all criteria has been matched, there is no point continuing
    // iterating the remaining functions.
    if (allMatched) return true;
  }
  return true;
}

bool Matcher::matchInstrsCriterion(FileLine criterion, MatchResult *result) {
  for (auto &F : _module->functions()) {
    if (skipFunction(&F)) continue;
    DISubprogram *SP = F.getSubprogram();
    if (spMatchFilename(SP, criterion.file.c_str())) {
      if (DEBUG_MATCHER) {
        dumpSP(SP);
      }
      unsigned start_line = SP->getLine();
      unsigned end_line = ScopeInfoFinder::getLastLine(&F);
      if (criterion.line >= start_line && criterion.line <= end_line) {
        result->matched =
            matchInstrsInFunction(criterion.line, &F, result->instrs);
        result->func = &F;
        return true;
      }
    }
  }
  return false;
}

Instruction *Matcher::matchInstr(FunctionInstSeq opt) {
  if (opt.function.empty() || opt.inst_no == 0) return nullptr;
  bool found = false;
  Function *F;
  for (Module::iterator I = _module->begin(), E = _module->end(); I != E; ++I) {
    F = &*I;
    if (!F->isDeclaration() && F->getName().equals(opt.function)) {
      found = true;
      break;
    }
  }
  if (!found) {
    return nullptr;
  }
  found = false;
  unsigned int inst_no = 0;
  for (inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ++ii) {
    inst_no++;  // instruction no. from 1 to N within the function F
    Instruction *instr = &*ii;
    if (opt.inst_no == inst_no) return instr;
  }
  return nullptr;
}

Instruction *Matcher::matchInstr(SmallVectorImpl<Instruction *> &candidates,
                                 std::string &target_instr_str, bool fuzzy,
                                 bool ignore_dbg, bool *is_result_exact) {
  Instruction *fuzzy_instr = nullptr;
  for (Instruction *instr : candidates) {
    std::string str_instr;
    llvm::raw_string_ostream rso(str_instr);
    instr->print(rso);
    trim(str_instr);
    if (target_instr_str.compare(str_instr) == 0) {
      // this is an exact match
      if (is_result_exact) *is_result_exact = true;
      return instr;
    } else {
      // If the string instruction does not match, we'll check
      // if the instruction can be fuzzily matched or matched
      // without the dbg slot number.
      if (fuzzy && fuzzilyMatch(str_instr, target_instr_str, ignore_dbg)) {
        fuzzy_instr = instr;
        // don't break here so that if we can find exact matching in the
        // loop, exact matching is still preferred
      }
    }
  }
  // this is a fuzzy result
  if (is_result_exact) *is_result_exact = false;
  return fuzzy_instr;
}

Instruction *Matcher::matchInstr(FileLine opt, std::string &instr_str,
                                 bool fuzzy, bool ignore_dbg,
                                 bool *is_result_exact) {
  if (instr_str.empty()) return nullptr;
  MatchResult result;
  if (!matchInstrsCriterion(opt, &result)) return nullptr;
  trim(instr_str);
  return matchInstr(result.instrs, instr_str, fuzzy, ignore_dbg,
                    is_result_exact);
}

bool Matcher::fuzzilyMatch(std::string &inst1_str, std::string &inst2_str,
                           bool ignore_dbg) {
  // The reason that the fuzzy matching is necessary is because
  // the debug information recorded in the Instrumenter is
  // based on the **instrumented** bitcode, which will shift
  // the register number in the assignment!!! If we are given the
  // original bitcode file, the register number will not match, e.g.,:
  //
  //  instr in original bitcode-
  //    %38 = load %struct.my_root*, %struct.my_root** %7, align 8, !dbg
  //    !73
  //  instr in instrumented bitcode-
  //    %41 = load %struct.my_root*, %struct.my_root** %7, align 8, !dbg
  //    !73
  //
  // They only differ by the register number! Besides the return value
  // register number, the argument register numbers could also change!

  // To implement fuzzy match, we split the instruction strings based on
  // the register regular expression, and then only if all the parts
  // of the split string match will the two strings match.

  if (ignore_dbg) {
    size_t dbg_pos1 = inst1_str.find("!dbg");
    size_t dbg_pos2 = inst2_str.find("!dbg");
    inst1_str = inst1_str.substr(0, dbg_pos1);
    inst2_str = inst2_str.substr(0, dbg_pos2);
  }

  std::sregex_token_iterator iter1(inst1_str.begin(), inst1_str.end(),
                                   register_regex, -1);
  std::sregex_token_iterator iter2(inst2_str.begin(), inst2_str.end(),
                                   register_regex, -1);
  std::sregex_token_iterator end;
  for (; iter1 != end && iter2 != end; ++iter1, ++iter2) {
    if (iter1->compare(*iter2) != 0) {
      break;
    }
  }
  if (iter1 == end && iter2 == end) {
    return true;
  }
  return false;
}

bool Matcher::matchWithoutDbg(std::string &inst1_str, std::string &inst2_str) {
  // here we assume the two input strings are not exact matching
  // in other words, the caller should take care of doing a
  // inst1_str.compare(inst2_str) == 0 test first before calling this function
  size_t dbg_pos1 = inst1_str.find("!dbg");
  size_t dbg_pos2 = inst2_str.find("!dbg");
  if (dbg_pos1 != string::npos && dbg_pos2 != string::npos) {
    // a quick check that if the dbg position is different
    // then they definitely won't match
    if (dbg_pos1 != dbg_pos2) return false;
    // slightly more efficient than using string.substr...
    size_t i = 0, j = 0;
    for (; i < dbg_pos1 && j < dbg_pos2; i++, j++) {
      if (inst1_str.at(i) != inst2_str.at(j)) return false;
    }
    return true;
  }
  // if one has !dbg and the other don't have a !dbg, we can
  // also try to compare, but it's safer to just reject them
  return false;
}
