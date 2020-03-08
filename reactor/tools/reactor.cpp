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

  // Step 0: Read static hook guid map file
  if (!PmemVarGuidMap::deserialize(options.hook_guid_file, varMap)) {
    fprintf(stderr, "Failed to parse hook GUID file %s\n",
            options.hook_guid_file);
    return 1;
  }
  printf("successfully parsed hook guid map with %lu entries\n", varMap.size());

  // Step 1: Read dynamic address trace file
  if (!PmemAddrTrace::deserialize(options.address_file, &varMap, addrTrace)) {
    fprintf(stderr, "Failed to parse hook GUID file %s\n",
            options.hook_guid_file);
    return 1;
  }
  printf("successfully parsed %lu dynamic address trace items\n",
         addrTrace.size());

  // Step 2: Opening Checkpoint Component PMEM File
  struct checkpoint_log *c_log =
      reconstruct_checkpoint(options.checkpoint_file, options.pmem_library);
  if (c_log == NULL) {
    fprintf(stderr, "abort checkpoint rollback operation\n");
    return 1;
  }
  printf("finished checkpoint reconstruction\n");

  // FIXME: remove, outdated
  // Step 2: Read dynamic address trace file
  FILE *fp;
  char line[100];
  fp = fopen(options.address_file, "r");
  if (fp == NULL) {
    perror("Error opening address file");
    return -1;
  }
  char *token;
  char *str_addresses[MAX_DATA];
  char *str_pool_address;
  int num_data = 0;

  while (fgets(line, 100, fp) != NULL) {
    token = strtok(line, ":");
    if (strcmp(token, "address") == 0) {
      token = strtok(NULL, ": ");
      str_addresses[num_data] = (char *)malloc(strlen(token) + 1);
      strcpy(str_addresses[num_data], token);
      str_addresses[num_data][strlen(str_addresses[num_data]) - 1] = '\0';
      num_data++;
    } else if (strcmp(token, "POOL address") == 0) {
      token = strtok(NULL, ": ");
      str_pool_address = (char *)malloc(strlen(token) + 1);
      strcpy(str_pool_address, token);
      str_pool_address[strlen(str_pool_address) - 1] = '\0';
    }
  }
  fclose(fp);

  // Step 3: Convert collected string addresses to pointers and offsets
  long long_addresses[MAX_DATA];
  void *addresses[MAX_DATA];
  long long_pool_address;
  void *pool_address;

  for (int i = 0; i < num_data; i++) {
    long_addresses[i] = strtol(str_addresses[i], NULL, 16);
    addresses[i] = (void *)long_addresses[i];
  }
  long_pool_address = strtol(str_pool_address, NULL, 16);
  pool_address = (void *)long_pool_address;

  // Step 4: Calculating offsets from pointers
  uint64_t offsets[MAX_DATA];
  void *pmem_addresses[MAX_DATA];
  PMEMobjpool *pop = pmemobj_open(options.pmem_file, options.pmem_layout);
  if (pop == NULL) {
    printf("could not open pop\n");
    return -1;
  }
  for (int i = 0; i < num_data; i++) {
    offsets[i] = (uint64_t)addresses[i] - (uint64_t)pool_address;
    pmem_addresses[i] = (void *)((uint64_t)pop + offsets[i]);
  }


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

  // Step 7: re-execution
  pmemobj_close(pop);
  fp = fopen(argv[5], "r");
  if (fp == NULL) {
    perror("Error opening file");
    return -1;
  }

  char *reexecution_lines[MAX_DATA];
  // int ret_val;
  // int reexecute = 0;
  int line_counter = 0;
  while (fgets(line, 100, fp) != NULL) {
    // printf("Retry attempt number %d\n", coarse_grained_tries);
    reexecution_lines[line_counter] = (char *)malloc(strlen(line) + 1);
    strcpy(reexecution_lines[line_counter], line);
    /*ret_val = system(line);
    printf("ret val of reexecution is %d\n", ret_val);
    if(ret_val != 1){
      reexecute = 1;
      break;
    }*/
    line_counter++;
  }

  printf("Reexecution %d: \n", coarse_grained_tries);
  printf("\n");
  re_execute(reexecution_lines, options.version_num, line_counter, addresses,
             c_log, pmem_addresses, num_data, options.pmem_file,
             options.pmem_layout, offsets);

  // free reexecution_lines and string arrays here
}
