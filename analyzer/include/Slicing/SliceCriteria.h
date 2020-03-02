// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _SLICING_SLICECRITERIA_H_
#define _SLICING_SLICECRITERIA_H_

#include <string>
#include <vector>

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"

namespace llvm {
namespace slicing {

struct SliceInstCriteriaOpt {
  std::string file_lines; // comma separated file:line criteria
  std::string inst; // string representation of the target instruction
  std::string func; // function name for the target instruction
  int inst_no; // target instruction is the i-th instruction in the given function

  SliceInstCriteriaOpt(std::string fileLines, std::string instruction,
                       std::string functionName, int instrNo)
      : file_lines(fileLines),
        inst(instruction),
        func(functionName),
        inst_no(instrNo) {}
};

bool parseSlicingCriteriaOpt(SliceInstCriteriaOpt &opt, Module &M, 
    std::vector<Instruction *> &match_insts);

} // namespace slicing
} // namespace llvm

#endif /* _SLICING_SLICECRITERIA_H_ */
