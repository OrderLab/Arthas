#include <iostream>

#include "Utils/Path.h"
#include "Utils/LLVM.h"

using namespace std;
using namespace llvm;

int main(int argc, char *argv[]) {


  cout << "left stripping 2 components from path a/b/c/d: " << stripname("a/b/c/d", 2) << endl;
  
  if (argc <= 1) {
    return 0;
  }

  LLVMContext context;
  unique_ptr<Module> M = parseModule(context, argv[1]);
  if (!M) {
    errs() << "Failed to parse '" << argv[1] << "' file:\n";
    return 1;
  }
  errs() << "Successfully parsed " << argv[1] << "\n";
}
