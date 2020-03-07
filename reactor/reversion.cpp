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

int main (int argc, char *argv[]){

  main_func(argc, argv);

}


