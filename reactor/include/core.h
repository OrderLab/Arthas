// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _REACTOR_CORE_H_
#define _REACTOR_CORE_H_

#include <libpmemobj.h>
#include <pthread.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "checkpoint.h"
#include "rollback.h"
#include "reactor-opts.h"
#include "DefUse/DefUse.h"
#include "Instrument/PmemAddrTrace.h"
#include "Instrument/PmemVarGuidMap.h"
#include "Matcher/Matcher.h"
#include "PMem/Extractor.h"
#include "Slicing/Slice.h"
#include "Slicing/SliceCriteria.h"
#include "Slicing/Slicer.h"
#include "Utils/LLVM.h"
#include "Utils/String.h"

#include "llvm/Support/FileSystem.h"

namespace arthas {

struct reaction_result {
  bool status;
  uint32_t trials;
};

class ReactorState {
 public:
  ReactorState(std::unique_ptr<llvm::LLVMContext> ctx)
      : ready(false), sys_module(nullptr), llvm_context(std::move(ctx)) {}

  ~ReactorState() {
    if (sys_module) delete sys_module.release();
  }

  bool ready;
  struct reactor_options options;
  std::unique_ptr<llvm::Module> sys_module;
  std::unique_ptr<llvm::LLVMContext> llvm_context;
  llvm::instrument::PmemVarGuidMap var_map;
  llvm::instrument::PmemAddrTrace addr_trace;
  llvm::matching::Matcher matcher;
  std::map<llvm::Function *, std::unique_ptr<llvm::pmem::PMemVariableLocator>>
      pmem_var_locator_map;
};

class Reactor {
 public:
  Reactor(std::unique_ptr<llvm::LLVMContext> ctx)
      : _state(llvm::make_unique<ReactorState>(std::move(ctx))) {}

  bool slice_fault_instr(llvm::slicing::Slices &slices,
                         llvm::Instruction *fault_inst);
  llvm::Instruction *locate_fault_instr(std::string &fault_loc,
                                        std::string &inst_str);
  bool prepare(int argc, char *argv[], bool server);
  bool react(std::string fault_loc, std::string inst_str,
             reaction_result *result);
  ReactorState *get_state() { return _state.get(); }

  static const char *get_checkpoint_file(const char *pmem_library);

 private:
  std::unique_ptr<ReactorState> _state;
};

class PmemAddrOffsetList {
 public:
  PmemAddrOffsetList(size_t size);
  ~PmemAddrOffsetList();

  size_t num_data;
  uint64_t *offsets;
  void **addresses;
  void **pmem_addresses;

  void **sorted_addresses;
  void **sorted_pmem_addresses;
};

}  // namespace arthas

#endif /* _REACTOR_CORE_H_ */
