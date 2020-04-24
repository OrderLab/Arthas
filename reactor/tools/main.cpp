#include "core.h"

using namespace std;
using namespace llvm;
using namespace arthas;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::instrument;
using namespace llvm::defuse;
using namespace llvm::matching;

int main(int argc, char *argv[]) {
  unique_ptr<Reactor> reactor = make_unique<Reactor>();
  if (!reactor->prepare(argc, argv)) {
    cerr << "Failed to prepare reactor, abort\n";
    exit(1);
  }
  struct reaction_result result;
  struct reactor_options &options = reactor->get_state()->options;
  if (!reactor->react(options.fault_loc, options.fault_instr, &result)) {
    cerr << "Failed to react on " << options.fault_instr << "\n";
    exit(1);
  }
  cout << "Successfully reacted on " << options.fault_instr << " in "
       << result.trials << "trials\n";
  return 0;
}
