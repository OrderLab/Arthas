// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include <libpmemobj.h>
#include <pthread.h>
#include <fstream>
#include <iostream>
#include <string>

#include "checkpoint.h"
#include "rollback.h"

#include "reactor-opts.h"

#include "Instrument/PmemAddrTrace.h"
#include "Instrument/PmemVarGuidMap.h"
#include "Slicing/Slice.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;
using namespace llvm::instrument;

PmemVarGuidMap varMap;
PmemAddrTrace addrTrace;

struct reactor_options options;

void parse_args(int argc, char *argv[]) {
  program = argv[0];
  if (!parse_options(argc, argv, options)) {
    fprintf(stderr, "Failed to parse the command line options\n");
    usage();
    exit(1);
  }
  if (strcmp(options.pmem_library, "libpmem") == 0) {
    options.checkpoint_file = "/mnt/pmem/pmem_checkpoint.pm";
  } else if (strcmp(options.pmem_library, "libpmemobj") == 0) {
    options.checkpoint_file = "/mnt/pmem/checkpoint.pm";
  } else {
    fprintf(stderr, "Unrecognized pmem library %s\n", options.pmem_library);
    usage();
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  parse_args(argc, argv);

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
  free(total_size);

  //revert_by_seq_num();

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
  coarse_grain_reversion(addresses, c_log, pmem_addresses, options.version_num,
                         num_data);
  pmemobj_close(pop);

  // Step 7: re-execution
  re_execute(options.reexecute_cmd, options.version_num, addresses, c_log,
             pmem_addresses, num_data, options.pmem_file, options.pmem_layout,
             offsets);
  free(addresses);
  free(pmem_addresses);
  free(offsets);
  // free reexecution_lines and string arrays here
}
