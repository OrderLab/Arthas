// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Slicing/SliceCriteria.h"
#include "Matcher/Matcher.h"
#include "Utils/String.h"

#include "llvm/Support/raw_ostream.h"

using namespace std;
using namespace llvm;
using namespace llvm::matching;

bool llvm::slicing::parseSlicingCriteriaOpt(
    SliceInstCriteriaOpt &opt, Module &M,
    std::vector<Instruction *> &match_insts) {

  bool found = false;
  if (!opt.inst.empty()) {
    // if the string representation of an instruction is provided, trim it
    // so that when we compare it with an instruction, spaces are ignored.
    trim(opt.inst);
  }
  if (!opt.file_lines.empty()) {
    vector<FileLine> fileLines;
    if (!FileLine::fromCriteriaStr(opt.file_lines, fileLines) || 
        fileLines.empty()) {
      errs() << "Failed to parse slicing criteria " << opt.file_lines << "\n";
      return false;
    }
    Matcher matcher;
    matcher.process(M);
    vector<MatchResult> matchResults;
    if (!matcher.matchInstrsCriteria(fileLines, matchResults)) {
      errs() << "Failed to find the slicing target instructions in module ";
      errs() << M.getName() << " from criteria " << opt.file_lines << "\n";
      return false;
    }
    for (MatchResult &result : matchResults) {
      if (opt.inst.empty()) {
        // if the string form of instruction is not specified, we will treat
        // all the instructions in this file:line to be matching
        for (Instruction *instr : result.instrs) {
          errs() << "Found slice instruction " << *instr << "\n";
          match_insts.push_back(instr);
        }
        found = true;
      } else {
        // if the string form of instruction is specified, we will only
        // match if an instruction's string form matches
        Instruction *instr = matcher.matchInstr(
            result.instrs, opt.inst, opt.fuzzy_match, opt.ignore_dbg);
        if (instr != nullptr) {
          match_insts.push_back(instr);
          errs() << "Found slice instruction " << *instr << "\n";
          found = true;
        }
      }
    }
    if (!found) {
      errs() << "Failed to find the target instruction " << opt.inst << "\n";
    }
    return found;
  }
  if (opt.func.empty()) {
    errs() << "Warning: no slicing criteria supplied through command line.\n";
    return false;
  }
  if (opt.inst.empty() || opt.inst_no <= 0) {
    errs() << "Must specify slice instruction when giving a slice function\n";
    return false;
  }
  Function *F;
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    F = &*I;
    if (!F->isDeclaration() && F->getName().equals(opt.func)) {
      found = true;
      break;
    }
  }
  if (!found) {
    errs() << "Failed to find slice function " << opt.func << "\n";
    return false;
  }
  found = false;
  int inst_no = 0;
  for (inst_iterator ii = inst_begin(F), ie = inst_end(F); ii != ie; ++ii) {
    inst_no++; // instruction no. from 1 to N within the function F
    Instruction * instr = &*ii;
    // if SliceInstNo is specified, we match the instruction simply based
    // on the instruction no.
    if (opt.inst_no > 0) {
      if (opt.inst_no == inst_no) {
        errs() << "Found slice instruction " << instr << "\n";
        match_insts.push_back(instr);
        found = true;
        break;
      }
    } else { // else we match by the instruction string representation
      std::string str_instr;
      llvm::raw_string_ostream rso(str_instr);
      instr->print(rso);
      trim(str_instr);
      if (opt.inst.compare(str_instr) == 0) {
        errs() << "Found slice instruction " << str_instr << "\n";
        match_insts.push_back(instr);
        found = true;
        break;
      }
    }
  }
  if (!found) {
    errs() << "Failed to find slice instruction ";
    if (opt.inst_no > 0) {
      errs() << "no. " << opt.inst_no;
    } else {
      errs() << opt.inst;
    }
    errs() << " in function " << opt.func << "\n";
    return false;
  }
  return true;
}
