#include <stdio.h>
#include "libpmemobj.h"
#include <stdlib.h>
#include "checkpoint_generic.h"

//C Implementation of Reverter because libpmemobj in c++
//expected dated pmem file version. Unable to backwards convert
int coarse_grained_tries = 0;
int fine_grained_tries = 0;

#define MAX_COARSE_ATTEMPTS 5
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

void coarse_grain_reversion(void ** addresses, struct checkpoint_log *c_log,
void **pmem_addresses, int version_num, int num_data){
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

  //Actual reversion
  int ind = -1;
  for(int i = 0; i < num_data; i++){
    size_t size = c_log->c_data[c_data_indices[i]].size[version_num];
    ind = search_for_address(addresses[i], size, c_log);
    printf("ind is %d for %p\n", ind, addresses[i]);
    revert_by_address(addresses[i], pmem_addresses[i], ind, version_num, 0, size, c_log );
    printf("AFTER REVERSION coarse value is %f or %d\n", *((double *)pmem_addresses[i]),
        *((int *)pmem_addresses[i]));
  }
  coarse_grained_tries++;

}

PMEMobjpool * redo_pmem_addresses(char *path, char *layout, int num_data,
void **pmem_addresses, uint64_t * offsets){
  PMEMobjpool *pop = pmemobj_open(path, layout);
  if(pop == NULL){
    printf("could not open pop\n");
    return -1;
  }
  for(int i = 0; i < num_data; i++){
    pmem_addresses[i] = (void *)((uint64_t) pop + offsets[i]);
  }
  return pop;
}



void re_execute(char **reexecution_lines, int version_num, int line_counter,
void ** addresses, struct checkpoint_log *c_log, void **pmem_addresses, int num_data,
char * path, char * layout, uint64_t *offsets){ 
  int ret_val;
  int reexecute_flag = 0;
  for(int i = 0; i < line_counter; i++){
    ret_val = system(reexecution_lines[i]);
    printf( "********************\n");
    printf("ret val is %d reexecute is %d\n", ret_val, reexecute_flag);
    if(WIFEXITED(ret_val)){
      printf("WEXITSTATUS OS %d\n", WEXITSTATUS(ret_val));
      if(WEXITSTATUS(ret_val) < 0 || WEXITSTATUS(ret_val) > 1){
        reexecute_flag = 1;
        break;
      }
    }
  } 
  if(coarse_grained_tries == MAX_COARSE_ATTEMPTS){
     return;
   }
  //Try again if we need to re-execute
  if(reexecute_flag){
    printf("try reversion again\n");
    PMEMobjpool *pop = redo_pmem_addresses(path, layout, num_data,
    pmem_addresses, offsets);
    coarse_grain_reversion(addresses, c_log, pmem_addresses, version_num - 1, num_data);
    pmemobj_close(pop);
    re_execute(reexecution_lines, version_num - 1, line_counter, addresses,
    c_log, pmem_addresses, num_data, path, layout, offsets);
  }
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
  //To be deleted: This will be unnecessary once data types are printed 
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
    revert_by_address(addresses[i], pmem_addresses[i], ind, atoi(argv[4]), 0, size, c_log );
    printf("AFTER REVERSION coarse value is %f or %d\n", *((double *)pmem_addresses[i]),
        *((int *)pmem_addresses[i]));
  }*/
  coarse_grain_reversion(addresses, c_log, pmem_addresses, atoi(argv[4]), num_data);

  //Step 7: re-execution
  pmemobj_close(pop);
  fp = fopen(argv[5], "r");
  if(fp == NULL){
    perror("Error opening file");
    return -1;
  }

  char * reexecution_lines[MAX_DATA];
  int ret_val;
  int reexecute = 0;
  int line_counter = 0;
  while(fgets(line, 100, fp) != NULL){
    printf("before reexecution\n");
    reexecution_lines[line_counter] = malloc(strlen(line) + 1);
    strcpy(reexecution_lines[line_counter], line);
    /*ret_val = system(line);
    printf("ret val of reexecution is %d\n", ret_val);
    if(ret_val != 1){
      reexecute = 1;
      break;
    }*/
   line_counter++;
  }

  re_execute(reexecution_lines, atoi(argv[4]), line_counter,
  addresses, c_log, pmem_addresses, num_data, argv[2], argv[3], offsets);

  //free reexecution_lines and string arrays here
}

