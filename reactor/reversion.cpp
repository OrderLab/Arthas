#include <pthread.h>
#include <fstream>
#include <iostream>
#include <string>
#include "libpmemobj.h"
#define MAX_DATA 1000
#define FINE_GRAIN_ATTEMPTS 10

#include "Slicing/Slice.h"

using namespace std;
using namespace llvm;
using namespace llvm::slicing;

extern "C" {
#include "c_reversion.h"
}

#define MAX_DATA 1000
#define FINE_GRAIN_ATTEMPTS 10

using namespace std;

int main (int argc, char *argv[]){
  // main_func(argc, argv);

  // Step 2: Read printed out file
  ifstream file (argv[1]);
  string line;
  string addresses[MAX_DATA];
  int num_data = 0;
  string pool_address;
  
  if(!file.is_open()){
    cout << "unable to open file\n";
    return -1;
  }

  while(getline(file, line)){
    if(line.substr(0,8) == "address:"){
      addresses[num_data] = line.substr(9);
      //strcpy(addresses[num_data], line.substr(9));
      num_data++;
    }
    else if(line.substr(0,4) == "POOL"){
      pool_address = line.substr(14);
      //strcpy(pool_address, line.substr(14));
    }
  }
  
  long long_addresses[MAX_DATA];
  void *ptr_addresses[MAX_DATA];
  long long_pool_address;
  void * ptr_pool_address;

  for(int i = 0; i < num_data; i++){
    long_addresses[i] = stol(addresses[i], nullptr, 16);
    ptr_addresses[i] = (void *)long_addresses[i];
    cout << "ptr address is " << ptr_addresses[i] << "\n";
  }
  long_pool_address = stol(pool_address, nullptr, 16);
  ptr_pool_address = (void *)long_pool_address;
  cout << "pool address is " << ptr_pool_address << "\n";

  //Step 3: Calculating offsets from pointers
  uint64_t offsets[MAX_DATA];
  for(int i = 0; i < num_data; i++){
    offsets[i] = (uint64_t)ptr_addresses[i] - (uint64_t)ptr_pool_address;
    cout << "offset is " << offsets[i] << "\n";
  }

  //Step 1: Opening Checkpoint Component PMEM File
  // struct checkpoint_log *c_log = reconstruct_checkpoint();

  //Step 3: For each address, convert from string to void *,
  //then subtract from the pool_address to get the offset of
  //each data structure.
  /*
  void *ptr_addresses[MAX_DATA];
  for(int i = 0; i < num_data; i++){
    addresses[i] = addresses[i] - pool_address;
  }
  */

  Slices slices;
  
  //Step 4: For each printed address, revert each data structure
  //Fine-grained reversion
  /*DgSlices slices;
  for (auto i = slices.begin(); i != slices.end(); ++i){
    //Link slice nodes with addresses that were collected
    DgSlice *slice = *i;
    for(int j = 0; j < FINE_GRAIN_ATTEMPTS; j++){
      uint64_t fine_grain_address = search_for_fine_address(instructions, j, slice->dep_instrs, addresses);
      int variable_index = search_for_offset(pop, find_grain_address);
      revert_by_offset(fine_grain_address + pop, fine_grain_address, variable_index, argv[2], 0,
      c_log.c_data[variable_index].size);
    }
  }
  */

  //Try coarse-grained control, revert everything to a version
  //that is passed by an argument
  /*for(int i = 0; i < num_data; i++){
    //find the offset in checkpoint, need to modify for range-based
    int variable_index = search_for_offset(pop, addresses[i]);
    revert_by_offset(addresses[i] + pop, addresses[i], variable_index, argv[2], 0, 
    c_log.c_data[variable_index].size);
  }*/
  //TODO: Need to use detector to find if a run crashes to retry reversion with
  //a different version or different slices.
  //Need to identify different slices for reversion
}
