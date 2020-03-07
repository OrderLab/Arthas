#include <fstream>
#include <iostream>
#include "libpmemobj.h"
#include <pthread.h>
#include <string>
extern "C" {
  #include "c_reversion.h"
}

#define MAX_DATA 1000
#define FINE_GRAIN_ATTEMPTS 10

using namespace std;

/*struct checkpoint_log * reconstruct_checkpoint(){
  PMEMobjpool *pop = pmemobj_open("/mnt/mem/checkpoint.pm", "checkpoint");
  if(!pop){
   cout << "pool not found\n";
   cout << pmemobj_errormsg();
   return NULL;
  }
  PMEMoid oid = pmemobj_root(pop, sizeof(uint64_t));
  uint64_t *old_pool = (uint64_t *) pmemobj_direct(oid);
  cout << "old pool " << *old_pool << "\n";
  struct checkpoint_log *c_log;
  PMEMoid clog_oid = POBJ_FIRST_TYPE_NUM(pop, 0);
  c_log = (struct checkpoint_log *) pmemobj_direct(clog_oid);
  cout << "c log c data " << c_log->c_data[0].version << "\n";
  
  uint64_t offset;
  offset = (uint64_t)c_log->c_data - *old_pool;
  int variable_count = c_log->variable_count;
  for(int i = 0; i < variable_count; i++){
    for(int j = 0; j < c_log->c_data[i].version; j++){
      offset = (uint64_t)c_log->c_data[i].data[j] - *old_pool;
      c_log->c_data[i].data[j] = (void *)((uint64_t)c_log->c_data[i].data[j] + offset);
    }
  }

}*/

int main (int argc, char *argv[]){



  main_func(argc, argv);

  //Step 2: Read printed out file
  /*ifstream file (argv[1]);
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
  void * ptr_addresses[MAX_DATA];
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
  struct checkpoint_log *c_log = reconstruct_checkpoint();
  */
  //Step 4: For each printed address, revert each data structure
  //Fine-grained reversion
  /*
  //Get mapping of llvm Instruction in Slice to printed address
  Slice Iterator -> compare slice llvm values with printed llvm values
  if they are the same, then add a mapping of address to llvm value
  Output a list of mappings for the slice
  Slices slices;
  for(Slice *slice: slices){
    if(slice->persistence == SlicePersistence::Volatile){
      //Do nothing
    } else{
      ompare slice llvm values with printed persistent llvm values
      if they are the same, then add a mapping of address to llvm value
      Output a list of mappings for the slice
      Iterate through mappings
      revert_by_address();
    }
  }
    for(int j = 0; j < FINE_GRAIN_ATTEMPTS; j++){
      uint64_t fine_grain_address = search_for_fine_address(instructions, j, slice->dep_instrs, addresses);
      int variable_index = search_for_offset(pop, find_grain_address);
      revert_by_offset(fine_grain_address + pop, fine_grain_address, variable_index, argv[2], 0,
      c_log.c_data[variable_index].size);
    }
  }*/

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
