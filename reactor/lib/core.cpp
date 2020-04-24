// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "core.h"

using namespace std;
using namespace llvm;
using namespace arthas;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::instrument;
using namespace llvm::defuse;
using namespace llvm::matching;

uint32_t createDgFlags(struct dg_options &options) {
  uint32_t flags = 0;
  if (options.entry_only) flags |= SlicerDgFlags::ENTRY_ONLY;
  if (options.enable_pta) flags |= SlicerDgFlags::ENABLE_PTA;
  // if slice control is specified, we must enable control dependency
  // in the dependence graph even if enable_ctrl is not specified
  if (options.enable_ctrl || options.slice_ctrl)
    flags |= SlicerDgFlags::ENABLE_CONTROL_DEP;
  if (options.support_thread) flags |= SlicerDgFlags::SUPPORT_THREADS;
  if (options.intra_procedural) flags |= SlicerDgFlags::INTRA_PROCEDURAL;
  if (options.inter_procedural) flags |= SlicerDgFlags::INTER_PROCEDURAL;
  return flags;
}

bool Reactor::slice_fault_instr(Slices &slices, Instruction *fault_inst) {
  unique_ptr<DgSlicer> _dgSlicer =
      make_unique<DgSlicer>(_state->sys_module.get(), SliceDirection::Backward);
  // for intra-procedural slicing, uncomment the following:
  // uint32_t flags = SlicerDgFlags::ENABLE_PTA |
  //                 SlicerDgFlags::INTRA_PROCEDURAL |
  //                 SlicerDgFlags::SUPPORT_THREADS;
  uint32_t flags = createDgFlags(_state->options.dg_options);
  auto llvm_dg_options = _dgSlicer->createDgOptions(flags);
  _dgSlicer->computeDependencies(llvm_dg_options);

  Function *F = fault_inst->getFunction();
  auto li = _state->pmem_var_locator_map.find(F);
  PMemVariableLocator *locator;
  if (li == _state->pmem_var_locator_map.end()) {
    _state->pmem_var_locator_map.emplace(F, make_unique<PMemVariableLocator>());
    locator = _state->pmem_var_locator_map[F].get();
    locator->runOnFunction(*F);
  } else {
    locator = _state->pmem_var_locator_map[F].get();
  }

  uint32_t slice_id = 0;
  uint32_t dep_flags = DEFAULT_DEPENDENCY_FLAGS;
  if (_state->options.dg_options.slice_ctrl) {
    // if we specified slice control, add it to the slice flags
    dep_flags |= SliceDependenceFlags::CONTROL;
  }
  SliceGraph *sg = _dgSlicer->slice(fault_inst, slice_id,
                                    SlicingApproachKind::Storing, dep_flags);
  if (sg == nullptr) {
    errs() << "Failed to construct the slice graph for " << *fault_inst << "\n";
    return false;
  }
  auto &st = _dgSlicer->getStatistics();
  unique_ptr<SliceGraph> slice_graph(sg);
  errs() << "INFO: Sliced away " << st.nodesRemoved << " from " << st.nodesTotal
         << " nodes\n";
  errs() << "INFO: Slice graph has " << slice_graph->size() << " node(s)\n";
  slice_graph->sort();

  error_code ec;
  raw_fd_ostream out_stream("slices.log", ec, sys::fs::F_Text);
  slice_graph->computeSlices(slices);
  out_stream << "=================Slice graph " << slice_graph->slice_id();
  out_stream << "=================\n";
  out_stream << *slice_graph << "\n";
  out_stream << "=================Slice list " << slice_graph->slice_id();
  out_stream << "=================\n";
  for (Slice *slice : slices) {
    auto persistent_vars = locator->vars().getArrayRef();
    slice->setPersistence(persistent_vars);
    slice->dump(out_stream);
  }
  out_stream.close();
  return true;
}

Instruction *Reactor::locate_fault_instr(string &fault_loc, string &inst_str) {
  if (!_state->matcher.processed()) {
    errs() << "Matcher is not ready, cannot use it\n";
    return nullptr;
  }
  FileLine fileLine;
  size_t n = std::count(fault_loc.begin(), fault_loc.end(), ':');
  if (n == 1) {
    FileLine::fromCriterionStr(fault_loc, fileLine);
  } else if (n == 2) {
    size_t pos = fault_loc.rfind(':');
    FileLine::fromCriterionStr(fault_loc.substr(0, pos), fileLine);
  } else {
    errs() << "invalid fault location specifier " << fault_loc << "\n";
    return nullptr;
  }
  // enable fuzzy matching and ignore !dbg metadata if necessary
  return _state->matcher.matchInstr(fileLine, inst_str, true, true);
}

bool Reactor::prepare(int argc, char *argv[]) {
  program = argv[0];
  if (!parse_options(argc, argv, _state->options)) {
    fprintf(stderr, "Failed to parse the command line options\n");
    return false;
  }
  struct reactor_options &options = _state->options;
  if (options.pmem_library) {
    options.checkpoint_file = get_checkpoint_file(options.pmem_library);
    if (!options.checkpoint_file) return false;
  }
  if (!options.hook_guid_file) {
    errs() << "No hook GUID file specified, abort reaction\n";
    return false;
  }
  _state->sys_module = parseModule(_state->llvm_context, options.bc_file);
  _state->matcher.process(*_state->sys_module);

  // Step 1: Read static hook guid map file
  if (!PmemVarGuidMap::deserialize(options.hook_guid_file, _state->var_map)) {
    fprintf(stderr, "Failed to parse hook GUID file %s\n",
            options.hook_guid_file);
    return false;
  }
  printf("successfully parsed hook guid map with %lu entries\n",
         _state->var_map.size());

  // Step 2.a: Read dynamic address trace file
  if (!PmemAddrTrace::deserialize(options.address_file, &_state->var_map,
                                  _state->addr_trace)) {
    fprintf(stderr, "Failed to parse hook GUID file %s\n",
            options.hook_guid_file);
    return false;
  }
  printf("successfully parsed %lu dynamic address trace items\n",
         _state->addr_trace.size());

  // FIXME: should support libpmem reactor, which does not have a pool address.
  if (_state->addr_trace.pool_empty()) {
    fprintf(stderr, "No pool address found in the address trace file, abort\n");
    return false;
  }

  // Step 2.b: Convert collected addresses to pointers and offsets
  // FIXME: here, we are assuming the target program only has a single pool.
  if (!_state->addr_trace.calculatePoolOffsets()) {
    fprintf(stderr,
            "Failed to calculate the address offsets w.r.t the pool address in "
            "the address trace file, abort\n");
    return false;
  }

  // map address to instructions
  if (!_state->addr_trace.addressesToInstructions(&_state->matcher)) {
    fprintf(stderr, "Failed to translate address to instructions, abort\n");
    return false;
  }
  return true;
}

bool Reactor::react(std::string fault_loc, string inst_str,
                    reaction_result *result) {
  if (!_state->ready) {
    fprintf(stderr, "Reactor state is not ready, abort\n");
    return false;
  }
  struct reactor_options &options = _state->options;
  Instruction *fault_inst = locate_fault_instr(fault_loc, inst_str);
  if (!fault_inst) {
    fprintf(stderr, "Failed to locate the fault instruction\n");
    return false;
  }
  errs() << "Located fault instruction " << *fault_inst << "\n";
  Slices fault_slices;
  if (!slice_fault_instr(fault_slices, fault_inst)) {
    fprintf(stderr, "Failed to compute slices for the fault instructions\n");
    return false;
  }
  errs() << "Computed " << fault_slices.size()
         << " slices of the fault instruction\n";

  // PMEMobjpool *pop = pmemobj_open(options.pmem_file, options.pmem_layout);
  void *pop;
  size_t mapped_len;
  int is_pmem;
  if (strcmp(options.pmem_library, "libpmemobj") == 0)
    pop = (void *)pmemobj_open(options.pmem_file, options.pmem_layout);
  else if (strcmp(options.pmem_library, "libpmem") == 0)
    pop = (void *)pmem_map_file(options.pmem_file, PMEM_LEN, PMEM_FILE_CREATE,
                                0666, &mapped_len, &is_pmem);
  if (pop == NULL) {
    printf("Could not open pmem file %s to get pool start address\n",
           options.pmem_file);
    return -1;
  }

  // Step 2.c: Calculating offsets from pointers
  // FIXME: assuming last pool is the pool of the pmemobj_open
  PmemAddrPool &last_pool = _state->addr_trace.pool_addrs().back();
  if (last_pool.addresses.empty()) {
    fprintf(stderr, "Last pool %s has no associated addresses in the trace\n",
            last_pool.pool_addr->addr_str.c_str());
    return false;
  }
  printf("Pool %s has %lu associated addresses in the trace\n",
         last_pool.pool_addr->addr_str.c_str(), last_pool.addresses.size());

  size_t num_data = last_pool.addresses.size();
  uint64_t *offsets = (uint64_t *)malloc(num_data * sizeof(uint64_t));
  void **addresses = (void **)malloc(num_data * sizeof(void *));
  void **pmem_addresses = (void **)malloc(num_data * sizeof(void *));
  for (size_t i = 0; i < num_data; ++i) {
    offsets[i] = last_pool.addresses[i]->pool_offset;
    cout << "offset is " << offsets[i] << "\n";
    addresses[i] = (void *)last_pool.addresses[i]->addr;
    pmem_addresses[i] = (void *)((uint64_t)pop + offsets[i]);
  }
  if (strcmp(options.pmem_library, "libpmem") == 0) {
    offsets[num_data] = 0;
    addresses[num_data] = (void *)last_pool.pool_addr;
    pmem_addresses[num_data] = pop;
    num_data++;
  }

  // Step 3: Opening Checkpoint Component PMEM File
  struct checkpoint_log *c_log =
      reconstruct_checkpoint(options.checkpoint_file, options.pmem_library);
  if (c_log == NULL) {
    fprintf(stderr, "abort checkpoint rollback operation\n");
    free(addresses);
    free(pmem_addresses);
    free(offsets);
    return 1;
  }
  printf("finished checkpoint reconstruction\n");

  // Step 5: Fine-grain reversion
  // Step 5a: Create ordered list of checkpoint entries using logical seq num
  single_data ordered_data[MAX_VARIABLES];
  size_t *total_size = (size_t *)malloc(sizeof(size_t));
  *total_size = 0;
  order_by_sequence_num(ordered_data, total_size, c_log);
  int *reverted_sequence_numbers = (int *)malloc(sizeof(int) * *total_size);
  memset(reverted_sequence_numbers, 0, *total_size * sizeof(int));
  // Step 5b: Bring in Slice Graph, find starting point in
  // terms of sequence number (connect LLVM Node to seq number)
  int starting_seq_num = -1;

  // Step 5c: sort the addresses arrays by sequence number
  void **sorted_addresses = (void **)malloc(num_data * sizeof(void *));
  void **sorted_pmem_addresses = (void **)malloc(num_data * sizeof(void *));
  // printf("total size is %ld\n", *total_size);
  sort_by_sequence_number(addresses, ordered_data, *total_size, num_data,
                          sorted_addresses, pmem_addresses,
                          sorted_pmem_addresses, offsets);

  // Step 5d: revert by sequence number
  for (auto it = _state->addr_trace.begin(); it != _state->addr_trace.end();
       it++) {
    PmemAddrTraceItem *traceItem = *it;
    if (traceItem->instr == fault_inst) {
      for (int i = *total_size; i >= 0; i--) {
        if (traceItem->addr == (uint64_t)ordered_data[i].address) {
          starting_seq_num = ordered_data[i].sequence_number;
        }
      }
    }
  }
  int req_flag2 = 0;
  int *slice_seq_numbers = (int *)malloc(sizeof(int) * 20);
  int slice_seq_iterator = 0;
  if (starting_seq_num != -1) {
    slice_seq_iterator = 1;
    slice_seq_numbers[0] = starting_seq_num;
    reverted_sequence_numbers[starting_seq_num] = 1;
  }
  for (Slice *slice : fault_slices) {
    for (auto &slice_item : *slice) {
      auto dep_inst = slice_item.first;
      // Iterate through addTraceList, find relevant address
      // for dep_inst, find address inside of ordered_data,
      // find corresponding sequence numbers for address
      for (auto it = _state->addr_trace.begin(); it != _state->addr_trace.end();
           it++) {
        // cout << "Inside Trace\n";
        PmemAddrTraceItem *traceItem = *it;
        // errs() << *traceItem->instr << "\n";
        // errs() << *dep_inst << "\n";
        if (traceItem->instr == dep_inst) {
          cout << "FOUND INSTRUCTION\n";
          errs() << *traceItem->instr << "\n";
          errs() << *dep_inst << "\n";
          // We found the address for the instruction: traceItem->addr
          for (int i = *total_size; i >= 0; i--) {
            cout << traceItem->addr << " " << (uint64_t)ordered_data[i].address
                 << "\n";
            if (traceItem->addr == (uint64_t)ordered_data[i].address &&
                ordered_data[i].sequence_number != starting_seq_num &&
                reverted_sequence_numbers[ordered_data[i].sequence_number] !=
                    1) {
              cout << "add to vector " << ordered_data[i].address << " "
                   << ordered_data[i].sequence_number << "\n";
              slice_seq_numbers[slice_seq_iterator] =
                  ordered_data[i].sequence_number;
              slice_seq_iterator++;
              reverted_sequence_numbers[ordered_data[i].sequence_number] = 1;
            }
          }
        }
      }
    }
    // TODO: Testing of this needs to be done.
    // Here we should do reversion on collected seq numbers and try
    // try reexecution
    int *decided_slice_seq_numbers = (int *)malloc(sizeof(int) * 20);
    int *decided_total = (int *)malloc(sizeof(int));
    *decided_total = 0;
    cout << "decision func\n";
    decision_func_sequence_array(slice_seq_numbers, slice_seq_iterator,
                                 decided_slice_seq_numbers, decided_total);
    cout << "revert by seq num\n";
    revert_by_sequence_number_array(sorted_pmem_addresses, ordered_data,
                                    decided_slice_seq_numbers, *decided_total);
    if (strcmp(options.pmem_library, "libpmemobj") == 0)
      pmemobj_close((PMEMobjpool *)pop);
    if (*decided_total > 0) {
      req_flag2 = re_execute(
          options.reexecute_cmd, options.version_num, addresses, c_log,
          pmem_addresses, num_data, options.pmem_file, options.pmem_layout,
          offsets, FINE_GRAIN, starting_seq_num, sorted_pmem_addresses,
          ordered_data, (void *)last_pool.pool_addr->addr);
    }
    if (req_flag2 == 1) {
      cout << "reversion with sequence numbers array has succeeded\n";
      return 1;
    }
    /*else {
      cout << "reversion did not work\n";
    }*/
    // if (!pop) {
    if (strcmp(options.pmem_library, "libpmemobj") == 0)
      pop = (void *)redo_pmem_addresses(options.pmem_file, options.pmem_layout,
                                        num_data, pmem_addresses, offsets);
    //}

    if (starting_seq_num != -1)
      slice_seq_iterator = 1;
    else
      slice_seq_iterator = 0;
  }

  cout << "start regular reversion\n";
  // Maybe: put in loop alongside reexecution, decrementing
  // most likely sequence number to rollback.

  // TODO: What to do if starting seq num is not the fault instruction?
  starting_seq_num = 19;
  int curr_version = ordered_data[starting_seq_num].version;
  revert_by_sequence_number_nonslice((void *)last_pool.pool_addr->addr,
                                     ordered_data, starting_seq_num,
                                     curr_version - 1, pop);
  if (strcmp(options.pmem_library, "libpmemobj") == 0)
    pmemobj_close((PMEMobjpool *)pop);
  int req_flag = re_execute(
      options.reexecute_cmd, options.version_num, addresses, c_log,
      pmem_addresses, num_data, options.pmem_file, options.pmem_layout, offsets,
      COARSE_GRAIN_SEQUENCE, starting_seq_num - 1, sorted_pmem_addresses,
      ordered_data, (void *)last_pool.pool_addr->addr);
  if (req_flag == 1) {
    cout << "reversion with sequence numbers has succeeded\n";
    return 1;
  }

  free(total_size);

  // Step 6: Coarse-grain reversion
  // To be deleted: This will be unnecessary once data types are printed
  /*int c_data_indices[MAX_DATA];
  for(int i = 0; i < c_log->variable_count; i++){
    printf("coarse address is %p\n", c_log->c_data[i].address);
    for(int j = 0; j < num_data; j++){
      if(addresses[j] == c_log->c_data[i].address){
        printf("coarse value is %f or %d\n", *((double *)pmem_addresses[j]),
        *((int *)pmem_addresses[j]));
        c_data_indices[j] = i;
      }
    }
  }
  //Actual reversion, argv[4] represents what version to revert to
  int ind = -1;
  for(int i = 0; i < num_data; i++){
    size_t size = c_log->c_data[c_data_indices[i]].size[atoi(argv[4])];
    ind = search_for_address(addresses[i], size, c_log);
    printf("ind is %d for %p\n", ind, addresses[i]);
    revert_by_address(addresses[i], pmem_addresses[i], ind, atoi(argv[4]), 0,
  size, c_log );
    printf("AFTER REVERSION coarse value is %f or %d\n", *((double
  *)pmem_addresses[i]),
        *((int *)pmem_addresses[i]));
  }*/
  printf("Reversion attempt %d\n", coarse_grained_tries + 1);
  printf("\n");
  if (!pop) {
    if (strcmp(options.pmem_library, "libpmemobj") == 0)
      redo_pmem_addresses(options.pmem_file, options.pmem_layout, num_data,
                          pmem_addresses, offsets);
  }
  coarse_grain_reversion(addresses, c_log, pmem_addresses, options.version_num,
                         num_data, offsets);
  if (strcmp(options.pmem_library, "libpmemobj") == 0)
    pmemobj_close((PMEMobjpool *)pop);

  // Step 7: re-execution
  re_execute(options.reexecute_cmd, options.version_num, addresses, c_log,
             pmem_addresses, num_data, options.pmem_file, options.pmem_layout,
             offsets, COARSE_GRAIN_NAIVE, starting_seq_num,
             sorted_pmem_addresses, ordered_data,
             (void *)last_pool.pool_addr->addr);
  free(addresses);
  free(pmem_addresses);
  free(offsets);
  // free reexecution_lines and string arrays here
  return true;
}

const char *Reactor::get_checkpoint_file(const char *pmem_library) {
  // FIXME: ugly......
  if (strcmp(pmem_library, "libpmem") == 0) {
    return "/mnt/pmem/pmem_checkpoint.pm";
  } else if (strcmp(pmem_library, "libpmemobj") == 0) {
    return "/mnt/pmem/checkpoint.pm";
  } else {
    fprintf(stderr, "Unrecognized pmem library %s\n", pmem_library);
    return nullptr;
  }
}

void parse_args(int argc, char *argv[]) {}
