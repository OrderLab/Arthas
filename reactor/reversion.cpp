#include <fstream>
#include <iostream>
#include "libpmemobj.h"
#include <pthread.h>
#include <string>

#define MAX_DATA 1000

using namespace std;

int main (int argc, char *argv[]){

  //Step 1: Opening Checkpoint Component PMEM File
  PMEMobjpool *pop = pmemobj_open("/mnt/mem/checkpoint.pm", "checkpoint");
  if(!pop){
   cout << "pool not found\n";
   return -1;
  }
  //TODO: Read pop, reconstruct checkpoint data structure
  /*
    void *old_pool = pmemobj_root(pop, sizeof(void *))
    struct checkpoint_log c_log = old_pool + sizeof(void *);
    uint64_t offset;
    offset = (uint64_t)c_log.c_data - (uint64_t)(old_pool);
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
  for(int i = 0; i < num_data; i++){
    addresses[i] = addresses[i] - pool_address;
  }
  */

  //Step 4: For each printed address, revert each data structure
  /*for(int i = 0; i < num_data; i++){
    //find the offset in checkpoint, need to modify for range-based
    int variable_index = search_for_offset(pop, addresses[i]);
    revert_by_offset(addresses[i] + pop, addresses[i], variable_index, 0, 0, 
    c_log.c_data[variable_index].size);
  }*/
}