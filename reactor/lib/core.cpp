// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "core.h"
#include <unistd.h>
#include <chrono>

#define BATCH_REEXECUTION 1000000
//#define BATCH_REEXECUTION 1
//#define ROLLBACK_MODE 1
#define ROLLBACK_MODE 0

using namespace std;
using namespace llvm;
using namespace arthas;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::instrument;
using namespace llvm::defuse;
using namespace llvm::matching;

multimap<Instruction *, PmemAddrTraceItem *> trace_instrs;
int binary_success = -1;
int binary_reversion_count = 0;
int total_reverted_items = 0;
int binary_reverted_items = 0;
int total_reexecutions = 0;
FILE *fp;
//FILE *fp2;
int fd;

// #define DUMP_SLICES 1
#define BINARY_REVERSION_ATTEMPTS 2
SetVector<llvm::Value *> pmem_vars;

// Used to configure dependency graph flags
uint32_t createDgFlags(struct dg_options &options) {
  uint32_t flags = 0;
  if (options.entry_only) flags |= SlicerDgFlags::ENTRY_ONLY;
  if (options.enable_pta) flags |= SlicerDgFlags::ENABLE_PTA;
  // if slice control is specified, we must enable control dependency
  // in the dependence graph even if enable_ctrl is not specified
  if (options.enable_ctrl || options.slice_ctrl)
    flags |= SlicerDgFlags::ENABLE_CONTROL_DEP;
  if (options.support_thread) flags |= SlicerDgFlags::SUPPORT_THREADS;
  if (options.intra_procedural) flags |= SlicerDgFlags::INTRA_PROCEDURAL;
  if (options.inter_procedural) flags |= SlicerDgFlags::INTER_PROCEDURAL;
  return flags;
}

// computes the dependencies of the executable, you can specify
// individual flags you want for the dependencies
bool Reactor::compute_dependencies() {
  std::unique_lock<std::mutex> lk(_lock);
  if (_state->dependency_computed) {
    return true;
  }
  if (_state->computing_dependency) {
    // if someone is already computing the dependency, we should wait until
    // it's finished.
    _cv.wait(lk, [this] { return this->_state->dependency_computed; });
    // then we should just return as someone has computed the dependency for us
    // already...
    return true;
  }
  // dependency is not computed, and no one is computing the dependency,
  // we are the one responsible for doing it. first, set the flag
  _state->computing_dependency = true;

  // for intra-procedural slicing, uncomment the following:
  // uint32_t flags = SlicerDgFlags::ENABLE_PTA |
  //                 SlicerDgFlags::INTRA_PROCEDURAL |
  //                 SlicerDgFlags::SUPPORT_THREADS;
  uint32_t flags = createDgFlags(_state->options.dg_options);
  auto llvm_dg_options = _state->dg_slicer->createDgOptions(flags);
  bool ok = _state->dg_slicer->computeDependencies(llvm_dg_options);
  _state->dependency_computed = true;
  // now we are done with the dependency computation
  _state->computing_dependency = false;
  _cv.notify_all();
  return ok;
}

// Compute the slices (slices of nodes) of the given fault instruction
bool Reactor::slice_fault_instr(Slices &slices, Instruction *fault_inst) {
  if (!compute_dependencies()) {
    return false;
  }
  Function *F = fault_inst->getFunction();
  auto li = _state->pmem_var_locator_map.find(F);
  PMemVariableLocator *locator;
  if (li == _state->pmem_var_locator_map.end()) {
    _state->pmem_var_locator_map.emplace(
        F, llvm::make_unique<PMemVariableLocator>());
    locator = _state->pmem_var_locator_map[F].get();
    locator->runOnFunction(*F);
  } else {
    locator = _state->pmem_var_locator_map[F].get();
  }

  uint32_t slice_id = 0;
  uint32_t dep_flags = DEFAULT_DEPENDENCY_FLAGS;
  if (_state->options.dg_options.slice_ctrl) {
    // if we specified slice control, add it to the slice flags
    dep_flags |= SliceDependenceFlags::CONTROL;
  }
  SliceGraph *sg = _state->dg_slicer->slice(
      fault_inst, slice_id, SlicingApproachKind::Storing, dep_flags);
  if (sg == nullptr) {
    errs() << "Failed to construct the slice graph for " << *fault_inst << "\n";
    return false;
  }
  auto &st = _state->dg_slicer->getStatistics();
  unique_ptr<SliceGraph> slice_graph(sg);
  errs() << "INFO: Sliced away " << st.nodesRemoved << " from " << st.nodesTotal
         << " nodes\n";
  errs() << "INFO: Slice graph has " << slice_graph->size() << " node(s)\n";
  std::clock_t time_start = clock();
  slice_graph->sort();
  std::clock_t time_end = clock();
  errs() << "INFO: Sorted slice graph in "
         << 1000.0 * (time_end - time_start) / CLOCKS_PER_SEC << " ms\n";
  time_start = clock();
  slice_graph->computeSlices(slices);
  time_end = clock();
  errs() << "INFO: Computed slices from graph in "
         << 1000.0 * (time_end - time_start) / CLOCKS_PER_SEC << " ms\n";

  time_start = clock();
// wtf, dumping the slices to log wind up taking a lot of time,
// should only enable it in debugging mode
#ifdef DUMP_SLICES
  error_code ec;
  raw_fd_ostream out_stream("slices.log", ec, sys::fs::F_Text);
  out_stream << "=================Slice graph " << slice_graph->slice_id();
  out_stream << "=================\n";
  out_stream << *slice_graph << "\n";
  out_stream << "=================Slice list " << slice_graph->slice_id();
  out_stream << "=================\n";
#endif
  for (Slice *slice : slices) {
    slice->setPersistence(pmem_vars);
// slice->setPersistence(persistent_vars);
#ifdef DUMP_SLICES
    slice->dump(out_stream);
#endif
  }
  for (Slice *slice : slices) {
    slice->print_slice_persistence();
    break;
  }
#ifdef DUMP_SLICES
  out_stream.close();
#endif
  time_end = clock();
  errs() << "INFO: Dumped slices in "
         << 1000.0 * (time_end - time_start) / CLOCKS_PER_SEC << " ms\n";
  return true;
}

// Locates the fault instruction and finds the corresponding node
// using the line number and instruction. Uses fuzzy matching if 
// necessary
Instruction *Reactor::locate_fault_instr(string &fault_loc, string &inst_str) {
  if (!_state->matcher.processed()) {
    errs() << "Matcher is not ready, cannot use it\n";
    return nullptr;
  }
  FileLine fileLine;
  size_t n = std::count(fault_loc.begin(), fault_loc.end(), ':');
  if (n == 1) {
    FileLine::fromCriterionStr(fault_loc, fileLine);
  } else if (n == 2) {
    size_t pos = fault_loc.rfind(':');
    FileLine::fromCriterionStr(fault_loc.substr(0, pos), fileLine);
  } else {
    errs() << "invalid fault location specifier " << fault_loc << "\n";
    return nullptr;
  }
  // enable fuzzy matching and ignore !dbg metadata if necessary
  return _state->matcher.matchInstr(fileLine, inst_str, true, true);
}

bool Reactor::prepare(int argc, char *argv[], bool server) {
  program = argv[0];
  if (!parse_options(argc, argv, _state->options)) {
    cerr << "Failed to parse the command line options\n";
    return false;
  }
  _state->mode = server ? ReactorMode::SERVER : ReactorMode::STANDALONE;
  struct reactor_options &options = _state->options;
  if (!server) {
    // if we are not running in the server mode, we should check a few more
    // options including the fault instruction, which must be specified
    // up-front
    if (options.fault_instr.empty()) {
      cerr << "fault instruction string is not set, specify it with -i\n";
      usage();
      return false;
    }
    if (options.fault_loc.empty()) {
      cerr << "fault instruction location is not set, specify it with -c\n";
      usage();
      return false;
    }
  }
  if (options.pmem_library) {
    options.checkpoint_file = get_checkpoint_file(options.pmem_library);
    if (!options.checkpoint_file) return false;
  }
  if (!options.hook_guid_file) {
    errs() << "No hook GUID file specified, abort reaction\n";
    return false;
  }
  // Step 0: Parse bitcode file, warm-up matcher
  _state->sys_module = parseModule(*_state->llvm_context, options.bc_file);
  _state->matcher.process(*_state->sys_module);

  // Step 1: Read static hook guid map file
  if (!PmemVarGuidMap::deserialize(options.hook_guid_file, _state->var_map)) {
    cerr << "Failed to parse hook GUID file " << options.hook_guid_file << endl;
    return false;
  }
  printf("successfully parsed hook guid map with %lu entries\n",
         _state->var_map.size());

  if (!server) {
    // If we are in standalone mode, we should parse the address trace file
    // Otherwise, the trace file needs to be continuously read and parsed.

    // Step 2.a: Read dynamic address trace file
    if (!PmemAddrTrace::deserialize(options.address_file, &_state->var_map,
                                    _state->addr_trace)) {
      cerr << "Failed to parse hook GUID file " << options.hook_guid_file
           << endl;
      return false;
    }
    cout << "Successfully parsed " << _state->addr_trace.size()
         << " dynamic address trace items\n";
    // FIXME: should support libpmem reactor, which does not have a pool
    // address.
    if (_state->addr_trace.pool_empty()) {
      cerr << "No pool address found in the address trace file, abort\n";
      return false;
    }
    // Step 2.b: Convert collected addresses to pointers and offsets
    // FIXME: here, we are assuming the target program only has a single pool.
    if (!_state->addr_trace.calculatePoolOffsets()) {
      cerr << "Failed to calculate the address offsets w.r.t the pool address "
              "in "
              "the address trace file, abort\n";
      return false;
    }
    // map address to instructions
    if (!_state->addr_trace.addressesToInstructions(&_state->matcher)) {
      cerr << "Failed to translate address to instructions, abort\n";
      return false;
    }
    cout << "Address trace translated to LLVM instructions\n";
  }

  _state->dg_slicer = llvm::make_unique<DgSlicer>(_state->sys_module.get(),
                                                  SliceDirection::Backward);
  _state->ready = true;
  return true;
}

// Used to monitor the address trace to see if we are done parsing
// and analzying the address trace
bool Reactor::monitor_address_trace() {
  struct reactor_options &options = _state->options;
  std::ifstream addrfile(options.address_file);
  if (!addrfile.is_open()) {
    errs() << "Failed to open " << options.address_file
           << " for reading address trace file\n";
    return false;
  }
  long long start_pos = 0, end_pos = 0;
  string partial_line, line;
  bool partial = false;
  unsigned lineno = 0;
  useconds_t check_delay = 100000;  // 100ms
  while (true) {
    if (!addrfile) {
      addrfile.open(options.address_file);
      usleep(check_delay);
      continue;
    }
    addrfile.seekg(0, addrfile.end);
    end_pos = addrfile.tellg();
    if (end_pos < start_pos) {
      // the end position is smaller than the last read position
      // this indicates the file is probably truncated or recreated
      // we have to start over...
      start_pos = 0;
      std::lock_guard<std::mutex> lk(_trace_mu);
      _state->addr_trace.clear();
      _state->trace_ready = false;
    }
    if (end_pos == start_pos) {
      // the end position is the same as the last position
      // the file content did not change, we don't need to do anything
      if (start_pos > 0) {
        {
          std::lock_guard<std::mutex> lk(_trace_mu);
          _state->trace_ready = true;
          _trace_ready_cv.notify_all();
        }
      }
    } else if (end_pos > start_pos) {
      // only if end position is larger than last read position
      addrfile.seekg(start_pos, addrfile.beg);
      while (getline(addrfile, line)) {
        // we might be reading somewhere in between a line is written
        // completely into the address file, in this case, we should
        // store partial result and concatenate it next time!
        if (addrfile.eof() && !line.empty()) {
          partial_line.append(line);
          partial = true;
        } else {
          PmemAddrTraceItem *item = new PmemAddrTraceItem();
          bool ok = false;
          if (partial) {
            // previous line is partial, append current line to previous line
            partial_line.append(line);
            ok =
                PmemAddrTraceItem::parse(partial_line, *item, &_state->var_map);
            // clear partial line
            partial_line.erase();
          } else {
            ok = PmemAddrTraceItem::parse(line, *item, &_state->var_map);
          }
          if (ok) {
            _state->addr_trace.add(item);
          } else {
            errs() << "Unrecognized address trace item at line " << lineno
                   << ": " << line << "\n";
            delete item;
          }
          partial = false;
        }
      }
      start_pos = end_pos;
    }
    addrfile.close();
    usleep(check_delay);
    addrfile.open(options.address_file);
  }

  addrfile.close();
  return true;
}

// Waiting until the address trace is ready to perform some
// misc. work on the collected data
bool Reactor::wait_address_trace_ready() {
  std::unique_lock<std::mutex> lk(_trace_mu);
  if (_state->trace_processed) {
    // trace has been processed, go ahead
    return true;
  }
  if (_state->processing_trace) {
    _trace_processed_cv.wait(lk,
                             [this] { return this->_state->trace_processed; });
    return true;
  }
  _state->processing_trace = true;
  if (!_state->trace_ready) {
    printf("Parsing address trace...\n");
    _trace_ready_cv.wait(lk, [this] { return this->_state->trace_ready; });
    if (!_state->trace_ready) {
      cerr << "Failed to wait for address trace ready\n";
      return false;
    }
  }
  cout << "Successfully parsed " << _state->addr_trace.size()
       << " dynamic address trace items\n";
  if (_state->addr_trace.pool_empty()) {
    cerr << "No pool address found in the address trace file, abort\n";
    return false;
  }
  if (!_state->addr_trace.calculatePoolOffsets()) {
    cerr << "Failed to calculate the address offsets w.r.t the pool address "
            "in "
            "the address trace file, abort\n";
    return false;
  }
  if (!_state->addr_trace.addressesToInstructions(&_state->matcher)) {
    cerr << "Failed to translate address to instructions, abort\n";
    return false;
  }
  cout << "Address trace translated to LLVM instructions\n";
  printf("preparing addr trace for pmem instructions\n");
  for (auto it = _state->addr_trace.begin(); it != _state->addr_trace.end();
       it++) {
    PmemAddrTraceItem *traceItem = *it;
    trace_instrs.insert(
        pair<Instruction *, PmemAddrTraceItem *>(traceItem->instr, traceItem));
  }
  printf("Address trace translated and prepared\n");

  fp = fopen("output_log", "w+");
  fprintf(fp, "Address trace ready\n");
  fflush(fp);
  //fp2 = fopen("results/result.txt", "a");

  _state->trace_processed = true;
  _state->processing_trace = false;
  _trace_processed_cv.notify_all();

  return true;
}

// Undo an array of sequence numbers
void undo_by_sequence_number_array(seq_log *s_log, std::vector<int> &seq_list) {
  for (int i = 0; i < (int)seq_list.size(); i++) {
    single_data search_data = lookup(s_log, seq_list[i]);
    undo_by_sequence_number(search_data, seq_list[i]);
  }
}

/* Binary Reversion Function to reduce data loss */
int binary_reversion(std::vector<int> &seq_list, int l, int r, seq_log *s_log,
                     void **pop, checkpoint_log *c_log, int num_data,
                     void *pool_address, struct reactor_options &options) {
  int decided_total, req_flag2;
  int *decided_slice_seq_numbers = (int *)malloc(sizeof(int) * seq_list.size());
  int starting_seq_num = 0;
  if (r >= l) {
    int mid = l + (r - l) / 2;
    auto first_left = seq_list.begin();
    vector<int>::iterator last_left = seq_list.begin() + (mid + 1);
    vector<int> left(first_left, last_left);

    vector<int>::iterator first_right;
    if (mid == 0 && seq_list.size() == 1)
      first_right = seq_list.begin() + (mid);
    else
      first_right = seq_list.begin() + (mid + 1);
    auto last_right = seq_list.end();
    vector<int> right(first_right, last_right);

    int *slice_seq_numbers = (int *)malloc(sizeof(int) * right.size());
    printf("slice seq numbers %p %d\n", slice_seq_numbers, (int)right.size());
    copy(right.begin(), right.end(), slice_seq_numbers);
    decided_total = 0;
    decision_func_sequence_array(slice_seq_numbers, right.size(),
                                 decided_slice_seq_numbers, &decided_total);

    printf("reverting %d\n", decided_total);
    binary_reverted_items = decided_total;
    revert_by_sequence_number_array(s_log,
                                    decided_slice_seq_numbers, decided_total,
                                    c_log);
    if (strcmp(options.pmem_library, "libpmemobj") == 0) {
      pmemobj_close((PMEMobjpool *)*pop);
    }
    if (decided_total > 0) {
      req_flag2 =
          re_execute(options.reexecute_cmd, options.version_num,
                     c_log, num_data, options.pmem_file,
                     options.pmem_layout, FINE_GRAIN, starting_seq_num,
                     pool_address, s_log, options.pmem_library);
      total_reexecutions++;
    }
    if (strcmp(options.pmem_library, "libpmemobj") == 0)
      *pop = redo_pmem_addresses(options.pmem_file, options.pmem_layout,
                                 num_data, s_log, options.pmem_library);
    binary_reversion_count++;
    // Check if succeeded
    if (req_flag2 == 1) {
      cout << "reversion with sequence numbers array has succeeded\n";
      printf("binary reversion count %d\n", binary_reversion_count);
      if (binary_reversion_count > BINARY_REVERSION_ATTEMPTS ||
          right.size() == 1) {
        binary_reversion_count = 0;
        binary_success = 1;
        return 0;
      }
      printf("begin undo\n");
      undo_by_sequence_number_array(s_log, right);

      free(decided_slice_seq_numbers);
      free(slice_seq_numbers);
      binary_reversion(right, 0, right.size() - 1, s_log, pop, c_log, num_data,
                       pool_address, options);
    } else {
      cout << "reversion has failed\n";
      printf("begin undo\n");

      undo_by_sequence_number_array(s_log, right);

      if (binary_reversion_count > BINARY_REVERSION_ATTEMPTS ||
          left.size() == 1) {
        binary_reversion_count = 0;
        // Reversion + re-execution of left side.
        int slice_seq_numbers[left.size()];
        copy(left.begin(), left.end(), slice_seq_numbers);
        decided_total = 0;
        decision_func_sequence_array(slice_seq_numbers, left.size(),
                                     decided_slice_seq_numbers, &decided_total);
        printf("reverting %d\n", decided_total);
        binary_reverted_items = decided_total;

        revert_by_sequence_number_array(s_log,
                                        decided_slice_seq_numbers,
                                        decided_total, c_log);
        if (strcmp(options.pmem_library, "libpmemobj") == 0)
          pmemobj_close((PMEMobjpool *)*pop);
        if (decided_total > 0) {
          req_flag2 = re_execute(
              options.reexecute_cmd, options.version_num, c_log,
              num_data, options.pmem_file, options.pmem_layout,
              FINE_GRAIN, starting_seq_num, pool_address, s_log,
              options.pmem_library);
          total_reexecutions++;
        }
        if (strcmp(options.pmem_library, "libpmemobj") == 0)
          *pop = redo_pmem_addresses(options.pmem_file, options.pmem_layout,
                                     num_data, s_log, options.pmem_library);
        if (req_flag2 == 1) {
          cout << "reversion with sequence numbers array has succeeded\n";
          binary_success = 1;
        }
        return 1;
      }
      free(decided_slice_seq_numbers);
      free(slice_seq_numbers);
      binary_reversion(left, 0, left.size() - 1, s_log, pop, c_log, num_data,
                       pool_address, options);
    }
  }
  return -1;
}

// Step 4a: Create hashmap of checkpoint entries where logical seq num
// is the key
void Reactor::seq_log_creation(seq_log * &s_log, size_t * &total_size,
                               seq_log * &r_log, struct checkpoint_log *c_log) {
  // Sequence log creation and handling
  s_log->size = LOG_SIZE;
  s_log->list =
      (struct seq_node **)malloc(sizeof(struct seq_node *) * LOG_SIZE);
  int i;
  for (i = 0; i < LOG_SIZE; i++) s_log->list[i] = NULL;

  // Ordering by sequence number and then initializing the r_log
  *total_size = 0;
  order_by_sequence_num(s_log, total_size, c_log);
  r_log->size = *total_size;
  r_log->list =
      (struct seq_node **)malloc(sizeof(struct seq_node) * *total_size);
  printf("total size is %d\n", (int)*total_size);
  for (int i = 0; i < (int)*total_size; i++) {
    r_log->list[i] = NULL;
  }

}

// Step 4b: Create hashmap of checkpoint entries where transaction id
// is the key
void Reactor::tx_log_creation(tx_log *t_log, struct checkpoint_log *c_log) {
  t_log->size = LOG_SIZE;
  t_log->list = (struct tx_node **)malloc(sizeof(struct tx_node *) * LOG_SIZE);
  for (int i = 0; i < LOG_SIZE; i++) t_log->list[i] = NULL;

  order_by_tx_id(t_log, c_log);
  int pos = txhashCode(t_log, 1);
  struct tx_node *t_list = t_log->list[pos];
  struct tx_node *t_temp = t_list;
  while (t_temp) {
    printf("data is %f or %d tx id is %d\n",
           *((double *)t_temp->ordered_data.data),
           *((int *)t_temp->ordered_data.data), t_temp->ordered_data.tx_id);
    t_temp = t_temp->next;
  }

}

// Step 4c: create offset sequence mapping
void Reactor::offset_seq_creation(multimap<uint64_t, int> &offset_seq_map,
                struct checkpoint_log *c_log, seq_log * &s_log) {
  std::cout << "before sort by seq num\n";
  struct seq_node *list;
  struct seq_node *temp;
  for (int i = 0; i < (int)s_log->size; i++) {
    list = s_log->list[i];
    temp = list;
    while (temp) {
      offset_seq_map.insert(pair<uint64_t, int>(temp->ordered_data.offset,
                                                temp->sequence_number));
      temp = temp->next;
    }
  }
}

// Step 4c: sort the addresses arrays by sequence number
void sort_creation(PmemAddrOffsetList &addr_off_list,
                            size_t num_data, struct checkpoint_log *c_log,
                            multimap<uint64_t, int> &offset_seq_map,
                            seq_log * &s_log){
  for (int j = 0; j < (int)num_data; j++) {
    typedef std::multimap<uint64_t, int>::iterator MMAPIterator;
    std::pair<MMAPIterator, MMAPIterator> result =
        offset_seq_map.equal_range(addr_off_list.offsets[j]);
    // Iterate over the range
    for (MMAPIterator it = result.first; it != result.second; it++) {
      lookup_modify(s_log, it->second, addr_off_list.pmem_addresses[j]);
    }
  }
  std::cout << "finished multimap iteration\n";
}

// Step 4c: sort the addresses arrays by sequence number
void address_seq_creation(multimap<const void *, int> &address_seq_nums,
                          seq_log * &s_log, int * & sequences, int * highest_num,
                          std::unique_ptr<ReactorState> & _state,
                          int *starting_seq_num, Instruction *fault_inst){
  struct seq_node *list;
  struct seq_node *temp;
  int ind = 0;
  for (int i = 0; i < (int)s_log->size; i++) {
    list = s_log->list[i];
    temp = list;
    while (temp) {
      address_seq_nums.insert(pair<const void *, int>(
          temp->ordered_data.address, temp->sequence_number));
      temp = temp->next;
    }
  }

  typedef std::multimap<const void *, int>::iterator MMAPIterator2;
  for (auto it = _state->addr_trace.begin(); it != _state->addr_trace.end();
       it++) {
    PmemAddrTraceItem *traceItem = *it;
    if (traceItem->instr == fault_inst) {
      std::pair<MMAPIterator2, MMAPIterator2> result =
          address_seq_nums.equal_range((const void *)traceItem->addr);
      // Iterate over the range
      ind = 0;
      *highest_num = -1;
      for (MMAPIterator2 it = result.first; it != result.second; it++) {
        sequences[ind] = it->second;
        if (sequences[ind] > *highest_num) *highest_num = sequences[ind];
        ind++;
      }
      if (*highest_num != -1) {
        *starting_seq_num = *highest_num;
      }
    }
  }
}

//finding smallest array elemnt
int findSmallestElement(int arr[], int n){
   int temp = arr[0];
   for(int i=0; i<n; i++) {
      if(temp>arr[i]) {
         temp=arr[i];
      }
   }
   return temp;
}

bool Reactor::react(std::string fault_loc, string inst_str,
                    reaction_result *result) {
  if (!_state->ready) {
    cerr << "Reactor state is not ready, abort\n";
    return false;
  }
  struct reactor_options &options = _state->options;
  Instruction *fault_inst = locate_fault_instr(fault_loc, inst_str);
  if (!fault_inst) {
    cerr << "Failed to locate the fault instruction\n";
    return false;
  }
  errs() << "Located fault instruction " << *fault_inst << "\n";
  Slices fault_slices;
  if (!slice_fault_instr(fault_slices, fault_inst)) {
    cerr << "Failed to compute slices for the fault instructions\n";
    return false;
  }

  errs() << "Computed " << fault_slices.size()
         << " slices of the fault instruction\n";
  if (!options.pmem_file) {
    cerr << "pmem file not specified, abort reversion\n";
    return false;
  }

  void *pop = NULL;
  size_t mapped_len;
  int is_pmem;
  if (strcmp(options.pmem_library, "libpmemobj") == 0)
    pop = (void *)pmemobj_open(options.pmem_file, options.pmem_layout);
  else if (strcmp(options.pmem_library, "libpmem") == 0)
    pop = (void *)pmem_map_file(options.pmem_file, PMEM_LEN, PMEM_FILE_CREATE,
                                0666, &mapped_len, &is_pmem);
  else if (strcmp(options.pmem_library, "mmap") == 0){
    fd = open(options.pmem_file, O_CREAT | O_RDWR, 0777);
    perror("eerror");
    if (fd == -1) printf("file opening did not work for some reason\n");
    pop = (void *)mmap(NULL, 100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  }
  if (pop == NULL) {
    cerr << "Could not open pmem file " << options.pmem_file
         << " to get pool start address\n";
    return -1;
  }
  printf("pop is %p\n", pop);
  int atemp = 3;
  memcpy(pop, &atemp, sizeof(int));
  // Step 2.c: Calculating offsets from pointers
  // FIXME: assuming last pool is the pool of the pmemobj_open
  PmemAddrPool &last_pool = _state->addr_trace.pool_addrs().back();
  if (last_pool.addresses.empty()) {
    cerr << "Last pool " << last_pool.pool_addr->addr_str
         << " has no associated addresses in the trace\n";
    return false;
  }
  printf("Pool %s has %lu associated addresses in the trace\n",
         last_pool.pool_addr->addr_str.c_str(), last_pool.addresses.size());

  size_t num_data = last_pool.addresses.size();
  PmemAddrOffsetList addr_off_list(num_data);
  for (size_t i = 0; i < num_data; ++i) {
    uint64_t offset = last_pool.addresses[i]->pool_offset;
    addr_off_list.offsets[i] = offset;
    addr_off_list.addresses[i] = (void *)last_pool.addresses[i]->addr;
    addr_off_list.pmem_addresses[i] = (void *)((uint64_t)pop + offset);
  }

  // Step 3: Opening Checkpoint Component PMEM File
  std::clock_t time_start = clock();
  struct checkpoint_log *c_log =
      reconstruct_checkpoint(options.checkpoint_file, options.pmem_library);
  if (c_log == NULL) {
    fprintf(stderr, "abort checkpoint rollback operation\n");
    return 1;
  }
  printf("finished checkpoint reconstruction\n");
  std::clock_t time_end = clock();
  errs() << "Checkpoint reconstruction took  "
         << 1000.0 * (time_end - time_start) / CLOCKS_PER_SEC << " ms\n";

  // Step 4: Fine-grain reversion
  // Step 4a: Create hashmap of checkpoint entries where logical seq num
  // is the key
  seq_log *s_log = (seq_log *)malloc(sizeof(seq_log));
  size_t *total_size = (size_t *)malloc(sizeof(size_t));
  seq_log *r_log = (seq_log *)malloc(sizeof(seq_log));
  seq_log_creation(s_log, total_size, r_log, c_log);

  // Step 4b: Create hashmap of checkpoint entries where transaction id
  // is the key
  tx_log *t_log = (tx_log *)malloc(sizeof(tx_log));
  tx_log_creation(t_log, c_log);

  // Step 5b: Bring in Slice Graph, find starting point in
  // terms of sequence number (connect LLVM Node to seq number)
  int starting_seq_num = -1;

  // Step 4c: sort the addresses arrays by sequence number
  addr_off_list.sorted_addresses = (void **)malloc(num_data * sizeof(void *));
  addr_off_list.sorted_pmem_addresses =
      (void **)malloc(num_data * sizeof(void *));
  time_start = clock();
  multimap<uint64_t, int> offset_seq_map;
  offset_seq_creation(offset_seq_map, c_log, s_log);
  time_end = clock();
  errs() << "Sort by seq num took  "
         << 1000.0 * (time_end - time_start) / CLOCKS_PER_SEC << " ms\n";

  std::cout << "insert into multimap" << num_data << "\n";
  time_start = clock();
  sort_creation(addr_off_list, num_data, c_log, offset_seq_map, s_log);
  time_end = clock();
  errs() << "Multimap iteration took  "
         << 1000.0 * (time_end - time_start) / CLOCKS_PER_SEC << " ms\n";
  time_start = clock();


  int *sequences = (int *)malloc(sizeof(int) * s_log->size);
  multimap<const void *, int> address_seq_nums;
  int highest_num = -1;
  address_seq_creation(address_seq_nums, s_log, sequences, &highest_num,
                       _state, &starting_seq_num, fault_inst);
  typedef std::multimap<const void *, int>::iterator MMAPIterator2;
  int ind = 0;
  time_end = clock();
  errs() << "highest num/starting seq num took  "
         << 1000.0 * (time_end - time_start) / CLOCKS_PER_SEC << " ms\n";
  time_start = clock();

  int req_flag2 = 0;
  int *slice_seq_numbers = (int *)malloc(sizeof(int) * s_log->size);
  int slice_seq_iterator = 0;
  single_data empty_data;
  starting_seq_num = -1;
  if (starting_seq_num != -1) {
    slice_seq_iterator = 1;
    slice_seq_numbers[0] = starting_seq_num;
    insert(r_log, starting_seq_num, empty_data);
  }
  int high_num = find_highest_seq_num(s_log);
  time_end = clock();
  errs() << "zzz high num took  "
         << 1000.0 * (time_end - time_start) / CLOCKS_PER_SEC << " ms\n";
  time_start = clock();
  printf("zzz high num is %d\n", high_num);
  int *decided_slice_seq_numbers = (int *)malloc(sizeof(int) * s_log->size);

  time_end = clock();
  errs() << "pmem addr trace creation took  "
         << 1000.0 * (time_end - time_start) / CLOCKS_PER_SEC << " ms\n";
  time_start = clock();

  //ArCkpt
  //arckpt(high_num, decided_slice_seq_numbers);
  if(options.arckpt){
    printf("begin arcpkt\n");
    int cpkt_ind = high_num;
    while(cpkt_ind > 0){
      ind = 0;
      decided_slice_seq_numbers[ind] = cpkt_ind;
      cpkt_ind--;
      ind++;
      total_reverted_items++;
      total_reexecutions++;
      revert_by_sequence_number_array(s_log,
                                  decided_slice_seq_numbers, ind, c_log );
      if (strcmp(options.pmem_library, "libpmemobj") == 0)
              pmemobj_close((PMEMobjpool *)pop);
      req_flag2 = re_execute(
            options.reexecute_cmd, options.version_num, c_log,
            num_data, options.pmem_file, options.pmem_layout, FINE_GRAIN,
            starting_seq_num, (void *)last_pool.pool_addr->addr, s_log,
            options.pmem_library);
       if(req_flag2 ==1){
          printf("reversion has succeeded\n");
          fprintf(fp, "%d items reverted\n", total_reverted_items);
          fprintf(fp, "total re-executions is %d\n", total_reexecutions);
          fclose(fp);
          printf("reversion has succeeded\n");
          return 1;
       }
       pop = (void *)redo_pmem_addresses(options.pmem_file, options.pmem_layout,
                                          num_data, s_log, options.pmem_library);
    }
    printf("finished arcpkt\n");
    return 1;
  }


  // std::set <Instruction *> found_instructions;
  vector<int> many_address_seq;
  int it_count = 0;
  int slice_id = 0;
  bool many_address_clear = false;
  for (Slice *slice : fault_slices) {
    cout << "Slice " << slice_id << "\n";
    slice_id++;
    for (auto &slice_item : *slice) {
      auto dep_inst = slice_item.first;

      auto result = trace_instrs.equal_range(dep_inst);
      for (auto trace_it = result.first; trace_it != result.second;
           ++trace_it) {
        // Iterate through addTraceList, find relevant address
        // for dep_inst, find address inside of ordered_data,
        // find corresponding sequence numbers for address
        PmemAddrTraceItem *traceItem = trace_it->second;
        if (traceItem->instr == dep_inst) {
          std::pair<MMAPIterator2, MMAPIterator2> result =
              address_seq_nums.equal_range((const void *)traceItem->addr);
          // Iterate over the range
          ind = 0;
          for (MMAPIterator2 it = result.first; it != result.second; it++) {
            sequences[ind] = it->second;
            ind++;
          }
          sort(sequences, sequences + ind, greater<int>());
          for (int i = ind - 1; i >= 0; i--) {
            int search_num = rev_lookup(r_log, sequences[i]);
            if (sequences[i] != -1 && search_num != 1) {
              /*single_data search_data = lookup(s_log, sequences[i]);
              // Figure out how to only revert the most recent sequence num
              printf("sequence number is %d\n", sequences[i]);
              struct node *found_node = lookup_clog(search_data.offset, c_log);
              if(found_node == NULL){
                printf("NULL FOUND NODe\n");
              }
	      else{
                int data_index = found_node->c_data.version;
                for(int j = 0; j <= data_index; j++){
                  printf("version is %d size is %ld seq num is %d value is %f or %d or %s\n",
                                       j, found_node->c_data.size[j],
                                       found_node->c_data.sequence_number[j],
                                       *((double *)found_node->c_data.data[j]),
                                       *((int *)found_node->c_data.data[j]),
                                       (char *)found_node->c_data.data[j]);
                }
              }*/
              if (i == 0) it_count++;
              many_address_seq.push_back(sequences[i]);
              slice_seq_numbers[slice_seq_iterator] = sequences[i];
              slice_seq_iterator++;
              insert(r_log, sequences[i], empty_data);
            }
          }
          // one-by-one reversion
          // onebyoneReversion();
        }  // if(traceItem->instr == dep_inst)
      }    // for (auto trace_it = result.first

      // Binary reversion for too many addresses
      if (many_address_seq.size() > BATCH_REEXECUTION) {

        printf("many address size is %d\n", (int)many_address_seq.size());
        printf("item count is %d\n", it_count);
        sort(many_address_seq.begin(), many_address_seq.end());

        printf("binary rev\n");
        binary_success = -1;
        binary_reversion(many_address_seq, 0, many_address_seq.size() - 1,
                             s_log, (void **)&pop, c_log, num_data,
                             (void *)last_pool.pool_addr->addr, options);
        total_reverted_items += binary_reverted_items;
        printf("done with binary reversion %d\n", binary_success);
        printf("total reverted items is %d\n", total_reverted_items);
        printf("total re-executions is %d\n", total_reexecutions);
	//fprintf(fp2, "%d items reverted\n", total_reverted_items);
	//fprintf(fp2, "total items is %d\n", (int)*total_size);
	//fflush(fp2);
	//fclose(fp2);
        if (binary_success == 1){
          fprintf(fp, "%d items reverted\n", total_reverted_items);
          fprintf(fp, "total re-executions is %d\n", total_reexecutions);
          fclose(fp);
          return 1;
        }
        many_address_seq.clear();
        many_address_clear = true;
      }
      // Here we should do reversion on collected seq numbers and try
      // try reexecution
      if (slice_seq_iterator >= 1 && many_address_seq.size() < BATCH_REEXECUTION && !many_address_clear) {
        int *decided_slice_seq_numbers =
            (int *)malloc(sizeof(int) * s_log->size);
        int *decided_total = (int *)malloc(sizeof(int));
        *decided_total = 0;
        decision_func_sequence_array(slice_seq_numbers, slice_seq_iterator,
                                     decided_slice_seq_numbers, decided_total);
        revert_by_sequence_number_array(s_log, decided_slice_seq_numbers,
                                        *decided_total, c_log);
        if(ROLLBACK_MODE){
          int lowest_number = findSmallestElement(decided_slice_seq_numbers,
                                                   *decided_total);
          int *rollback_seq_numbers = (int *)malloc(sizeof(int) * s_log->size);
          int total_rollback = 0;
          for(int i = high_num - 1; i >= lowest_number; i--){
            int s_num = rev_lookup(r_log, i);
            if(s_num != 1){
              rollback_seq_numbers[total_rollback] = i;
              total_rollback++;
              insert(r_log, i, empty_data);
            }
          }
          revert_by_sequence_number_array(s_log, rollback_seq_numbers,
                                          total_rollback, c_log);
          high_num = lowest_number;
        }
        // Function that iterates through decided slice seq numbers
        // gets the tx_ids associated with them, also reverts those values.
        /* revert_by_transaction(addr_off_list.sorted_pmem_addresses, t_log,
                             decided_slice_seq_numbers, *decided_total,
           s_log);*/
        if (*decided_total > 0) {
          if (strcmp(options.pmem_library, "libpmemobj") == 0)
            pmemobj_close((PMEMobjpool *)pop);
          req_flag2 = re_execute(
              options.reexecute_cmd, options.version_num, c_log,
              num_data, options.pmem_file, options.pmem_layout,
              FINE_GRAIN, starting_seq_num, (void *)last_pool.pool_addr->addr,
              s_log, options.pmem_library);
          total_reexecutions++;
         if (strcmp(options.pmem_library, "libpmemobj") == 0)
            pop = (void *)redo_pmem_addresses(
              options.pmem_file, options.pmem_layout, num_data,
              s_log, options.pmem_library);
          else if (strcmp(options.pmem_library, "mmap") == 0){
            fd = open(options.pmem_file, O_CREAT | O_RDWR, 0777);
            perror("eerror");
            if (fd == -1) printf("file opening did not work for some reason\n");
            pop = (void *)mmap(NULL, 100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            struct seq_node *list;
            struct seq_node *temp;
            for (int i = 0; i < (int)s_log->size; i++) {
              list = s_log->list[i];
              temp = list;
              while (temp) {
                temp->ordered_data.sorted_pmem_address =
                 (void *)((uint64_t)pop + temp->ordered_data.offset);
                temp = temp->next;
               }
             }
          }
          total_reverted_items += *decided_total;
        }
        if (req_flag2 == 1) {
          cout << "reversion with sequence numbers array has succeeded\n";
          printf("total re-executions is %d\n", total_reexecutions);
          return 1;
        }
        free(decided_slice_seq_numbers);
      }  // if(slice_seq_iterator >= 1 && many_address_seq.size() < 11)
      if (starting_seq_num != -1)
        slice_seq_iterator = 1;
      else
        slice_seq_iterator = 0;
      if(many_address_clear)
        many_address_clear = false;
    }  // for (auto &slice_item : *slice)
  }    // for (Slice *slice : fault_slices)

  cout << "start regular reversion\n";
  return 1;
}

const char *Reactor::get_checkpoint_file(const char *pmem_library) {
  // FIXME: ugly.....
  printf("pmem library is %s\n", pmem_library);
  if (strcmp(pmem_library, "libpmem") == 0 ||
      strcmp(pmem_library, "libpmemobj") == 0 ||
      strcmp(pmem_library, "mmap") == 0) {
    return "/mnt/pmem/pmem_checkpoint.pm";
  } else if (strcmp(pmem_library, "libpmemobj2") == 0) {
    return "/mnt/pmem/checkpoint.pm";
  } else {
    fprintf(stderr, "Unrecognized pmem library %s\n", pmem_library);
    return nullptr;
  }
}

PmemAddrOffsetList::PmemAddrOffsetList(size_t size) {
  num_data = size;
  offsets = (uint64_t *)malloc(num_data * sizeof(uint64_t));
  addresses = (void **)malloc(num_data * sizeof(void *));
  pmem_addresses = (void **)malloc(num_data * sizeof(void *));
  sorted_addresses = nullptr;
  sorted_pmem_addresses = nullptr;
}

PmemAddrOffsetList::~PmemAddrOffsetList() {
  if (offsets) free(offsets);
  if (addresses) free(addresses);
  if (pmem_addresses) free(pmem_addresses);
  if (sorted_addresses) free(sorted_addresses);
  if (sorted_pmem_addresses) free(sorted_pmem_addresses);
}
