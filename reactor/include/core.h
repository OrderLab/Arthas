// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _REACTOR_CORE_H_
#define _REACTOR_CORE_H_


#define LOG_SIZE 16000010
#include <libpmemobj.h>
#include <pthread.h>
#include <algorithm>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

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
#include "checkpoint.h"
#include "reactor-opts.h"
#include "rollback.h"

#include "llvm/Support/FileSystem.h"

namespace arthas {

struct reaction_result {
  bool status;
  uint32_t trials;
};

enum class ReactorMode { SERVER, STANDALONE };

class ReactorState {
 public:
  ReactorState(std::unique_ptr<llvm::LLVMContext> ctx)
      : mode(ReactorMode::STANDALONE), ready(false), sys_module(nullptr),
        llvm_context(std::move(ctx)), dependency_computed(false),
        computing_dependency(false), trace_ready(false), trace_processed(false),
        processing_trace(false) {}

  ~ReactorState() {
    if (sys_module) delete sys_module.release();
  }

  ReactorMode mode;
  bool ready;
  struct reactor_options options;
  std::unique_ptr<llvm::slicing::DgSlicer> dg_slicer;
  std::unique_ptr<llvm::Module> sys_module;
  std::unique_ptr<llvm::LLVMContext> llvm_context;
  llvm::instrument::PmemVarGuidMap var_map;
  llvm::instrument::PmemAddrTrace addr_trace;
  llvm::matching::Matcher matcher;
  std::map<llvm::Function *, std::unique_ptr<llvm::pmem::PMemVariableLocator>>
      pmem_var_locator_map;
  bool dependency_computed;
  bool computing_dependency;
  bool trace_ready;
  bool trace_processed;
  bool processing_trace;
};

class Reactor {
 public:
  Reactor(std::unique_ptr<llvm::LLVMContext> ctx)
      : _state(llvm::make_unique<ReactorState>(std::move(ctx))) {}

  bool slice_fault_instr(llvm::slicing::Slices &slices,
                         llvm::Instruction *fault_inst);
  llvm::Instruction *locate_fault_instr(std::string &fault_loc,
                                        std::string &inst_str);
  bool monitor_address_trace();
  bool wait_address_trace_ready();

  bool prepare(int argc, char *argv[], bool server);
  void seq_log_creation(seq_log * &s_log, size_t * &total_size,
                        seq_log * &r_log, struct checkpoint_log *c_log);
  void tx_log_creation(tx_log *t_log, struct checkpoint_log *c_log);
  void offset_seq_creation(std::multimap<uint64_t, int> &offset_seq_map,
                struct checkpoint_log *c_log, seq_log * &s_log);
  bool react(std::string fault_loc, std::string inst_str,
             reaction_result *result);
  ReactorState *get_state() { return _state.get(); }

  static const char *get_checkpoint_file(const char *pmem_library);

  bool compute_dependencies();

 private:
  std::unique_ptr<ReactorState> _state;
  std::mutex _lock;
  std::condition_variable _cv;

  std::mutex _trace_mu;
  std::condition_variable _trace_ready_cv;
  std::condition_variable _trace_processed_cv;
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
