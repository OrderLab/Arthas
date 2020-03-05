#include <stdio.h>
#include "libpmemobj.h"
#include <stdlib.h>
#include "checkpoint_generic.h"

//C Implementation of Reverter because libpmemobj in c++
//expected dated pmem file version. Unable to backwards convert

#define MAX_DATA 1000

struct checkpoint_log * reconstruct_checkpoint(){
  PMEMobjpool *pop = pmemobj_open("/mnt/mem/checkpoint.pm", "checkpoint");
  if(!pop){
   printf("pool not found\n");
   pmemobj_errormsg();
   return NULL;
  }
  PMEMoid oid = pmemobj_root(pop, sizeof(uint64_t));
  uint64_t *old_pool = (uint64_t *) pmemobj_direct(oid);
  printf("old pool is %p\n", (void *)*old_pool);
  //cout << "old pool " << *old_pool << "\n";
  struct checkpoint_log *c_log;
  PMEMoid clog_oid = POBJ_FIRST_TYPE_NUM(pop, 0);
  c_log = (struct checkpoint_log *) pmemobj_direct(clog_oid);
  printf("c log variables is %d\n", c_log->variable_count);
  //cout << "c log c data " << c_log->c_data[0].version << "\n";

  uint64_t offset;
  offset = (uint64_t)c_log->c_data - *old_pool;
  int variable_count = c_log->variable_count;
  for(int i = 0; i < variable_count; i++){
    for(int j = 0; j <= c_log->c_data[i].version; j++){
      offset = (uint64_t)c_log->c_data[i].data[j] - *old_pool;
      printf("offset is %ld\n", offset);
      c_log->c_data[i].data[j] = (void *)((uint64_t)pop + offset);
    }
  }

  for(int i =0; i < variable_count; i++){
    printf("address is %p\n", c_log->c_data[i].address);
    printf("version is %d\n", c_log->c_data[i].version);
    int data_index = c_log->c_data[i].version;
    for(int j = 0; j <= data_index; j++){
      printf("version is %d, value is %f or %d\n", j, *((double *)c_log->c_data[i].data[j]),
      *((int *)c_log->c_data[i].data[j]));
    }
  }
  *old_pool = (uint64_t)pop;
  return c_log;
}

int main(int argc, char *argv[]){
  
  //Step 1: Opening Checkpoint Component PMEM File
  struct checkpoint_log *c_log = reconstruct_checkpoint();
  printf("finished checkpoint reconstruction\n");

  //Step 2: Read printed out file
  FILE *fp;
  char line[100];
  fp = fopen(argv[1], "r");
  if(fp == NULL){
    perror("Error opening file");
    return -1;
  }
  char * token;
  char *str_addresses[MAX_DATA];
  char *str_pool_address;
  int num_data = 0;

  while(fgets(line, 100, fp) != NULL){
    token = strtok(line, ":");
    if(strcmp(token, "address") == 0){
      token = strtok(NULL, ": ");
      str_addresses[num_data] = malloc(strlen(token) + 1);
      strcpy(str_addresses[num_data],token);
      str_addresses[num_data][strlen(str_addresses[num_data]) - 1] = '\0';
      num_data++;
    }
    else if(strcmp(token, "POOL address") == 0){
      token = strtok(NULL, ": ");
      str_pool_address = malloc(strlen(token) + 1);
      strcpy(str_pool_address, token);
      str_pool_address[strlen(str_pool_address) - 1] = '\0';
    }
  }
  fclose(fp);

  //Step 3: Convert collected string addresses to pointers and offsets
  long long_addresses[MAX_DATA];
  void * addresses[MAX_DATA];
  long long_pool_address;
  void * pool_address;

  for(int i = 0; i < num_data; i++){
    long_addresses[i] = strtol(str_addresses[i], NULL, 16);
    addresses[i] = (void *)long_addresses[i];
  }
  long_pool_address = strtol(str_pool_address, NULL, 16);
  pool_address = (void *)long_pool_address;

  //Step 4: Calculating offsets from pointers
  uint64_t offsets[MAX_DATA];
  void *pmem_addresses [MAX_DATA];
  PMEMobjpool *pop = pmemobj_open(argv[2], argv[3]);
  if(pop == NULL){
    printf("could not open pop\n");
    return -1;
  }
  for(int i = 0; i < num_data; i++){
    offsets[i] = (uint64_t)addresses[i] - (uint64_t)pool_address;
    pmem_addresses[i] = (void *)((uint64_t)pop + offsets[i]);
  }

   
  //Step 5: Fine-grain reversion

  //Step 6: Coarse-grain reversion
  //TODO: Print data type in instrumentation
  int c_data_indices[MAX_DATA];
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

  int ind = -1;
  for(int i = 0; i < num_data; i++){
    size_t size = c_log->c_data[c_data_indices[i]].size[atoi(argv[4])];
    ind = search_for_address(addresses[i], size, c_log);
    printf("ind is %d for %p\n", ind, addresses[i]);
    revert_by_address(addresses[i], pmem_addresses[i], ind, atoi(argv[4]), 0, size, c_log );
    printf("AFTER REVERSION coarse value is %f or %d\n", *((double *)pmem_addresses[i]),
        *((int *)pmem_addresses[i]));
  }
}
