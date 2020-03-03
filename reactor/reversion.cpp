#include <fstream>
#include <iostream>
#include "libpmemobj.h"
#include <pthread.h>
#include <string>
#include "checkpoint_generic.h"

#define MAX_DATA 1000
#define FINE_GRAIN_ATTEMPTS 10

using namespace std;

int main (int argc, char *argv[]){

  //Step 1: Opening Checkpoint Component PMEM File
  PMEMobjpool *pop = pmemobj_open("/mnt/mem/checkpoint.pm", "checkpoint");
  if(!pop){
   cout << "pool not found\n";
   cout << pmemobj_errormsg();
   return -1;
  }
  PMEMoid oid = pmemobj_root(pop, sizeof(uint64_t));
  uint64_t *old_pool = (uint64_t *) pmemobj_direct(oid);
  cout << "old pool " << *old_pool << "\n";
  struct checkpoint_log *c_log;
  PMEMoid clog_oid = POBJ_FIRST_TYPE_NUM(pop, 0);
  c_log = (struct checkpoint_log *) pmemobj_direct(clog_oid);
  cout << "c log c data " << c_log->c_data[0].version << "\n";
 
  //TODO: Read pop, reconstruct checkpoint data structure
  /*
    void *old_pool = pmemobj_root(pop, sizeof(void *))
    struct checkpoint_log c_log = old_pool + sizeof(void *);
    uint64_t offset;
    offset = (uint64_t)c_log.c_data - (uint64_t)(old_pool);
    int variables = c_log.variables;
    for(int i = 0; i < variables; i++){
      offset = (uint64_t)c_log.c_data[i].address - (uint64_t)(old_pool);
      c_log.c_data[i].address = (void *)((uint64_t) c_log.c_data[i].address + offset);
      offset = (uint64_t)c_log.c_data[i].size - (uint64_t)(old_pool);
      c_log.c_data[i].size = (void *)((uint64_t)c_log.c_data[i].size + offset);
      for(int j = 0; j < version; j++){
        offset = (uint64_t)c_log.c_data[i].data[j] - (uint64_t)(old_pool);
        c_log.c_data[i].data[j] = (void *)((uint64_t)c_log.c_data[i].data[j] + offset);
      }
    }
  */
  
  //Step 2: Read printed out file
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
      //strcpy(addresses[num_data], line.substr(9));
      num_data++;
    }
    else if(line.substr(0,4) == "POOL"){
      //strcpy(pool_address, line.substr(14));
    }
  }
  
  //Step 3: For each address, convert from string to void *,
  //then subtract from the pool_address to get the offset of
  //each data structure.
  /*
  void *ptr_addresses[MAX_DATA];
  for(int i = 0; i < num_data; i++){
    addresses[i] = addresses[i] - pool_address;
  }
  */
  
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


/*uint64_t search_for_fine_address(llvm::SmallVector<const llvm::Instruction *> instructions, int index, llvm::SmallVector<const llvm::Instruction *> dep_instrs, string *addresses){
  int count = 0;
  int iteration = 0;
  for(auto i = instructions.begin(); i != instructions.end(); i++){
    llvm::instruction *inst = *i;
    if(inst == i){
      if(count < num)
        count++;
      else{
        return addresses[iteration];
      }
    }
    iteration++;
  }
}*/
