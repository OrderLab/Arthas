// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Instrument/PmemAddrTrace.h"
#include "Instrument/PmemVarGuidMap.h"
#include "Matcher/Matcher.h"
#include "Utils/String.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;
using namespace llvm;
using namespace llvm::instrument;
using namespace llvm::matching;

// Must be consistent with the field separator used in the address tracker lib
const char *PmemAddrTrace::FieldSeparator = ",";

// the pmemobj_create call instruction string to identify pool addresses
static const char *PmemObjCreateCallInstrStr =
    "call %struct.pmemobjpool* @pmemobj_create";

// the pmem_map_file call instruction string to identify mmap region
static const char *PmemCreateCallInstrStr = "call i8* @pmem_map_file";

PmemAddrTrace::~PmemAddrTrace() {
  for (auto item : _items) {
    delete item;
  }
}

bool PmemAddrTrace::deserialize(const char *fileName, PmemVarGuidMap *varMap,
                                PmemAddrTrace &result, bool ignoreBadLine) {
  std::ifstream addrfile(fileName);
  if (!addrfile.is_open()) {
    errs() << "Failed to open " << fileName
           << " for reading address trace file\n";
    return false;
  }
  string line;
  unsigned lineno = 0;
  while (getline(addrfile, line)) {
    lineno++;
    vector<string> parts;
    splitList(line, FieldSeparator, parts);
    if (parts.size() != EntryFields) {
      errs() << "Unrecognized line " << lineno << ", expecting " << EntryFields
             << " fields got " << parts.size() << " fields:" << line << "\n";
      if (!ignoreBadLine) return false;
      continue;
    }
    PmemAddrTraceItem *item = new PmemAddrTraceItem();
    item->addr_str = parts[0];
    // convert the hex address into a decimal uint64 value
    if (item->addr_str[0] == '0' &&
        (item->addr_str[1] == 'x' || item->addr_str[1] == 'X')) {
      // remove the 0x or 0X prefix in the address string
      item->addr = str2fmt<uint64_t>(item->addr_str.substr(2), true);
    } else {
      // the address string is not prefixed with 0x or 0X
      item->addr = str2fmt<uint64_t>(item->addr_str, true);
    }
    item->guid = str2fmt<uint64_t>(parts[1]);
    if (varMap != nullptr) {
      // if the GUID map is supplied, we'll resolve the corresponding pmem
      // variable information from the map with the GUID
      auto vi = varMap->find(item->guid);
      if (vi != varMap->end()) {
        item->var = &vi->second;
        // FIXME: for now we identify whether a trace item is from a pool
        // address
        // by checking the associated instruction string in the map.
        // Another way is to directly record a flag in the trace entry to
        // indicate whether the address is a pool address. This would
        // require modifying the instrumenter and address tracker lib API.
        if (item->var->instruction.find(PmemObjCreateCallInstrStr) !=
            string::npos) {
          errs() << "Found a pool address " << item->addr_str << "\n";
          item->is_pool = true;
        } else if (item->var->instruction.find(PmemCreateCallInstrStr) !=
                   string::npos) {
          errs() << "Found a libpmem file address " << item->addr_str << "\n";
          item->is_mmap = true;
        }
      }
    }
    result.add(item);
  }
  addrfile.close();
  return true;
}

bool PmemAddrTrace::addressesToInstructions(Matcher *matcher) {
  // assume the matcher has been called with process(Module) beforehand
  if (!matcher->processed()) {
    errs() << "Matcher is not ready, cannot use it\n";
    return false;
  }

  // each item has one file:line, reserve the vector
  for (auto &item : _items) {
    if (item->var == nullptr) continue;
    // FIXME: path + filename is probably better
    FileLine fileLine(item->var->source_file, item->var->line);
    errs() << "Source " << item->var->source_file << ":" << item->var->line
           << "\n";
    MatchResult matchResult;
    if (!matcher->matchInstrsCriterion(fileLine, &matchResult)) {
      errs() << "Failed to find instruction for address " << item->addr_str
             << ", guid " << item->guid << "\n";
      continue;
    }
    bool found = false;
    for (Instruction *instr : matchResult.instrs) {
      std::string str_instr;
      llvm::raw_string_ostream rso(str_instr);
      instr->print(rso);
      errs() << "Instruction " << *instr << "\n";
      if (item->var->instruction.compare(str_instr) == 0) {
        // update the instruction field of item
        item->instr = instr;
        errs() << "Found instruction for address " << item->addr_str
               << ", guid " << item->guid << ": " << str_instr << "\n";
        found = true;
        break;
      }
    }
    if (!found) {
      errs() << "Failed to find instruction for address " << item->addr_str
             << ", guid " << item->guid << ", instr " << item->var->instruction
             << "\n";
    }
  }
  return true;
}

bool PmemAddrTrace::calculatePoolOffsets() {
  // if we don't have any poor address, we can't calculate the offsets
  if (_pool_addrs.empty()) {
    return false;
  }
  // FIXME: here we are assuming that the addresses printed after a
  // pool address belong to that pool address, which may not be true at all...
  // To get the accurate offset requires more powerful instrumentation
  uint64_t last_pool_addr = 0;
  bool offset_obtained = false;
  PmemAddrPool *last_pool = nullptr;
  for (auto &item : _items) {
    // we treat the handling the mmap and pool offsets/addresses the same
    if (item->is_pool || item->is_mmap) {
      bool pool_found = false;
      for (auto &pool : _pool_addrs) {
        if (pool.pool_addr == item) {
          last_pool = &pool;
          last_pool_addr = item->addr;
          pool_found = true;
          break;
        }
      }
      assert(pool_found);
      continue;
    }
    // skip those addresses if the pool address has not be found
    if (last_pool == nullptr) continue;
    // calculate offset only if the address is larger than the last pool address
    if (item->addr >= last_pool_addr) {
      item->pool_offset = item->addr - last_pool_addr;
      // assume this item belongs to this pool
      last_pool->addresses.push_back(item);
      offset_obtained = true;
    }
  }
  return offset_obtained;
}
