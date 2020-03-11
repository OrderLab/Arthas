// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <libpmemobj.h>
#include <pthread.h>
#include <algorithm>
#include <fstream>
#include <iostream>
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

using namespace std;
using namespace llvm;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::instrument;
using namespace llvm::defuse;
using namespace llvm::matching;

Instruction *faultInstr;
PmemVarGuidMap varMap;
PmemAddrTrace addrTrace;

struct reactor_options options;

unique_ptr<SliceGraph> instructionSlice(Instruction *fault_inst,
                                        PMemVariableLocator &locator,
                                        Slices &slices,
                                        unique_ptr<DgSlicer> &_dgSlicer) {
  // Take faulty instruction and walk through Dependency Graph to
  // obtain slices + metadata of persistent variables
  uint32_t slice_id = 0;
  SliceGraph *sg =
      _dgSlicer->slice(fault_inst, slice_id, SlicingApproachKind::Storing);
  auto &st = _dgSlicer->getStatistics();
  unique_ptr<SliceGraph> slice_graph(sg);
  errs() << "INFO: Sliced away " << st.nodesRemoved << " from " << st.nodesTotal
         << " nodes\n";
  errs() << "INFO: Slice graph has " << slice_graph->size() << " node(s)\n";

  slice_graph->computeSlices(slices);
  for (Slice *slice : slices) {
    auto persistent_vars = locator.vars().getArrayRef();
    slice->setPersistence(persistent_vars);
  }
  return slice_graph;
}

bool slice_fault_instruction(Module *M, Slices &slices,
                             Instruction *fault_inst) {
  unique_ptr<DgSlicer> _dgSlicer =
      make_unique<DgSlicer>(M, SliceDirection::Backward);
  _dgSlicer->computeDependencies();

  map<Function *, unique_ptr<PMemVariableLocator>> locatorMap;
  Function *F = fault_inst->getFunction();
  auto li = locatorMap.find(F);
  if (li == locatorMap.end()) {
    locatorMap.insert(make_pair(F, make_unique<PMemVariableLocator>()));
  }
  PMemVariableLocator *locator = locatorMap[F].get();
  locator->runOnFunction(*F);
  instructionSlice(fault_inst, *locator, slices, _dgSlicer);
  return true;
}

Instruction *locate_fault_instruction(Module *M, Matcher *matcher) {
  if (!matcher->processed()) {
    errs() << "Matcher is not ready, cannot use it\n";
    return nullptr;
  }
  FileLine fileLine;
  size_t n =
      std::count(options.fault_loc.begin(), options.fault_loc.end(), ':');

  if (n == 1) {
    FileLine::fromCriterionStr(options.fault_loc, fileLine);
  } else if (n == 2) {
    size_t pos = options.fault_loc.rfind(':');
    FileLine::fromCriterionStr(options.fault_loc.substr(0, pos), fileLine);
  } else {
    errs() << "invalid fault location specifier " << options.fault_loc << "\n";
    return nullptr;
  }
  return matcher->matchInstr(fileLine, options.fault_instr);
}

void parse_args(int argc, char *argv[]) {
  program = argv[0];
  if (!parse_options(argc, argv, options)) {
    fprintf(stderr, "Failed to parse the command line options\n");
    usage();
    exit(1);
  }
  if (options.pmem_library) {
    if (strcmp(options.pmem_library, "libpmem") == 0) {
      options.checkpoint_file = "/mnt/pmem/pmem_checkpoint.pm";
    } else if (options.pmem_library &&
               strcmp(options.pmem_library, "libpmemobj") == 0) {
      options.checkpoint_file = "/mnt/pmem/checkpoint.pm";
    } else {
      fprintf(stderr, "Unrecognized pmem library %s\n", options.pmem_library);
      usage();
      exit(1);
    }
  }
}

int main(int argc, char *argv[]) {
  parse_args(argc, argv);

  LLVMContext context;
  unique_ptr<Module> M = parseModule(context, options.bc_file);
  Matcher matcher;
  matcher.process(*M);

  faultInstr = locate_fault_instruction(M.get(), &matcher);
  if (!faultInstr) {
    errs() << "Failed to locate the fault instruction\n";
    return 1;
  }

  // Step 1: Read static hook guid map file
  if (!PmemVarGuidMap::deserialize(options.hook_guid_file, varMap)) {
    fprintf(stderr, "Failed to parse hook GUID file %s\n",
            options.hook_guid_file);
    return 1;
  }
  printf("successfully parsed hook guid map with %lu entries\n", varMap.size());

  // Step 2.a: Read dynamic address trace file
  if (!PmemAddrTrace::deserialize(options.address_file, &varMap, addrTrace)) {
    fprintf(stderr, "Failed to parse hook GUID file %s\n",
            options.hook_guid_file);
    return 1;
  }
  printf("successfully parsed %lu dynamic address trace items\n",
         addrTrace.size());

  // FIXME: should support libpmem reactor, which does not have a pool address.
  if (addrTrace.pool_empty()) {
    fprintf(stderr, "No pool address found in the address trace file, abort\n");
    return 1;
  }

  // Step 2.b: Convert collected addresses to pointers and offsets
  // FIXME: here, we are assuming the target program only has a single pool.
  if (!addrTrace.calculatePoolOffsets()) {
    fprintf(stderr,
            "Failed to calculate the address offsets w.r.t the pool address in "
            "the address trace file, abort\n");
    return 1;
  }

  // map address to instructions
  addrTrace.addressesToInstructions(&matcher);

  // Step 2.c: Calculating offsets from pointers
  // FIXME: assuming last pool is the pool of the pmemobj_open
  PmemAddrPool &last_pool = addrTrace.pool_addrs().back();
  if (last_pool.addresses.empty()) {
    fprintf(stderr, "Last pool %s has no associated addresses in the trace\n",
            last_pool.pool_addr->addr_str.c_str());
    return 1;
  }
  printf("Pool %s has %lu associated addresses in the trace\n",
         last_pool.pool_addr->addr_str.c_str(), last_pool.addresses.size());
  PMEMobjpool *pop = pmemobj_open(options.pmem_file, options.pmem_layout);
  if (pop == NULL) {
    printf("Could not open pmem file %s to get pool start address\n",
           options.pmem_file);
    return -1;
  }
  size_t num_data = last_pool.addresses.size();
  uint64_t *offsets = (uint64_t *)malloc(num_data * sizeof(uint64_t));
  void **addresses = (void **)malloc(num_data * sizeof(void *));
  void **pmem_addresses = (void **)malloc(num_data * sizeof(void *));
  for (size_t i = 0; i < num_data; ++i) {
    offsets[i] = last_pool.addresses[i]->pool_offset;
    // cout << "offset is " << offsets[i] << "\n";
    addresses[i] = (void *)last_pool.addresses[i]->addr;
    pmem_addresses[i] = (void *)((uint64_t)pop + offsets[i]);
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

  // Step 5b: Bring in Slice Graph, find starting point in
  // terms of sequence number (connect LLVM Node to seq number)
  int starting_seq_num = -1;

  // Step 5c: sort the addresses arrays by sequence number
  void **sorted_addresses = (void **)malloc(num_data * sizeof(void *));
  void **sorted_pmem_addresses = (void **)malloc(num_data * sizeof(void *));
  printf("total size is %ld\n", *total_size);
  sort_by_sequence_number(addresses, ordered_data, *total_size, num_data,
                          sorted_addresses, pmem_addresses,
                          sorted_pmem_addresses, offsets);

  // Step 5d: revert by sequence number

  Slices slices;
  slice_fault_instruction(M.get(), slices, faultInstr);
  for (auto it = addrTrace.begin(); it != addrTrace.end(); it++) {
    PmemAddrTraceItem *traceItem = *it;
    if (traceItem->instr == faultInstr) {
      for (int i = *total_size; i >= 0; i--) {
        if (traceItem->addr == (uint64_t)ordered_data[i].address) {
          starting_seq_num = ordered_data[i].sequence_number;
        }
      }
    }
  }

  int *slice_seq_numbers = (int *)malloc(sizeof(int) * 20);
  int slice_seq_iterator = 0;
  if (starting_seq_num != -1) {
    slice_seq_iterator = 1;
    slice_seq_numbers[0] = starting_seq_num;
  }
  for (Slice *slice : slices) {
    for (auto dep_inst = slice->begin(); dep_inst != slice->end(); dep_inst++) {
      // Iterate through addTraceList, find relevant address
      // for dep_inst, find address inside of ordered_data,
      // find corresponding sequence numbers for address
      for (auto it = addrTrace.begin(); it != addrTrace.end(); it++) {
        PmemAddrTraceItem *traceItem = *it;
        if (traceItem->instr == *dep_inst) {
          // We found the address for the instruction: traceItem->addr
          for (int i = *total_size; i >= 0; i--) {
            if (traceItem->addr == (uint64_t)ordered_data[i].address &&
                ordered_data[i].sequence_number != starting_seq_num) {
              slice_seq_numbers[slice_seq_iterator] =
                  ordered_data[i].sequence_number;
              slice_seq_iterator++;
            }
          }
        }
      }
    }
    // TODO: Testing of this needs to be done.
    // Here we should do reversion on collected seq numbers and try
    // try reexecution
    revert_by_sequence_number_array(sorted_pmem_addresses, ordered_data,
                                    slice_seq_numbers, slice_seq_iterator);
    pmemobj_close(pop);
    int req_flag2 =
        re_execute(options.reexecute_cmd, options.version_num, addresses, c_log,
                   pmem_addresses, num_data, options.pmem_file,
                   options.pmem_layout, offsets, FINE_GRAIN, starting_seq_num,
                   sorted_pmem_addresses, ordered_data);
    if (req_flag2 == 1) {
      cout << "reversion with sequence numbers array has succeeded\n";
      return 1;
    }
    if (!pop) {
      redo_pmem_addresses(options.pmem_file, options.pmem_layout, num_data,
                          pmem_addresses, offsets);
    }
    if (starting_seq_num != -1)
      slice_seq_iterator = 1;
    else
      slice_seq_iterator = 0;
  }

  cout << "start regular reversion\n";
  // Maybe: put in loop alongside reexecution, decrementing
  // most likely sequence number to rollback.
  int curr_version = ordered_data[starting_seq_num].version;
  revert_by_sequence_number(sorted_pmem_addresses, ordered_data,
                            starting_seq_num, curr_version - 1);
  pmemobj_close(pop);
  int req_flag =
      re_execute(options.reexecute_cmd, options.version_num, addresses, c_log,
                 pmem_addresses, num_data, options.pmem_file,
                 options.pmem_layout, offsets, COARSE_GRAIN_SEQUENCE,
                 starting_seq_num - 1, sorted_pmem_addresses, ordered_data);
  if (req_flag) {
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
    redo_pmem_addresses(options.pmem_file, options.pmem_layout, num_data,
                        pmem_addresses, offsets);
  }
  coarse_grain_reversion(addresses, c_log, pmem_addresses, options.version_num,
                         num_data, offsets);
  pmemobj_close(pop);

  // Step 7: re-execution
  re_execute(options.reexecute_cmd, options.version_num, addresses, c_log,
             pmem_addresses, num_data, options.pmem_file, options.pmem_layout,
             offsets, COARSE_GRAIN_NAIVE, starting_seq_num,
             sorted_pmem_addresses, ordered_data);
  free(addresses);
  free(pmem_addresses);
  free(offsets);
  // free reexecution_lines and string arrays here
}
