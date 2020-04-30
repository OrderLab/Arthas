// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _INSTRUMENT_PMEMADDRTRACE_H_
#define _INSTRUMENT_PMEMADDRTRACE_H_

// Data structures to parse the dynamic pmem address trace file.
// The functionality should be ideally be provided by the address tracker
// library since the runtime library decides the trace format. But we don't
// want to link the tracker library with the reactor. So we implement
// the parser here for now.

#include <cassert>
#include <map>
#include <string>
#include <vector>

namespace llvm {

// forward declare the llvm::Instruction
class Instruction;

namespace matching {
class Matcher;
}

namespace instrument {

class PmemVarGuidMap;
class PmemVarGuidMapEntry;

class PmemAddrTraceItem {
 public:
  // string form of the dynamic address (in hex format)
  std::string addr_str;
  // convert the hex address string into a decimal uint64 address.
  uint64_t addr;
  // guid of the source instruction location
  uint64_t guid;
  // the offset within an owner pool address (default 0)
  uint64_t pool_offset;
  // if the address is a pool address or not
  bool is_pool;
  // if the address is a pmem file address or not
  bool is_mmap;
  // the associated guid map entry to locate the source instruction
  PmemVarGuidMapEntry *var;
  // the LLVM instruction responsible for generating the address
  llvm::Instruction *instr;

  static const char *FieldSeparator;
  // should be consistent with address tracker runtime lib
  static const int EntryFields = 2;

  PmemAddrTraceItem()
      : addr(0), guid(0), pool_offset(0), is_pool(false), is_mmap(false),
        var(nullptr), instr(nullptr) {}

  static bool parse(std::string &item_str, PmemAddrTraceItem &item,
                    PmemVarGuidMap *varMap = nullptr);
};

class PmemAddrPool {
 public:
  PmemAddrTraceItem *pool_addr;
  std::vector<PmemAddrTraceItem *> addresses;

  PmemAddrPool(PmemAddrTraceItem *pool) : pool_addr(pool) {}
};

class PmemAddrTrace {
 public:
  typedef std::vector<PmemAddrTraceItem *> TraceListTy;
  typedef std::vector<PmemAddrPool> TracePoolListTy;
  typedef TraceListTy::iterator iterator;
  typedef TraceListTy::const_iterator const_iterator;
  typedef TracePoolListTy::iterator pool_iterator;
  typedef TracePoolListTy::const_iterator const_pool_iterator;

 public:
  ~PmemAddrTrace();

  void add(PmemAddrTraceItem *item) {
    _items.push_back(item);
    if (item->is_pool || item->is_mmap) {
      _pool_addrs.push_back(PmemAddrPool(item));
    }
  }

  iterator begin() { return _items.begin(); }
  iterator end() { return _items.end(); }
  const_iterator begin() const { return _items.begin(); }
  const_iterator end() const { return _items.end(); }
  size_t size() const { return _items.size(); }
  TraceListTy &items() { return _items; }

  void clear();

  pool_iterator pool_begin() { return _pool_addrs.begin(); }
  pool_iterator pool_end() { return _pool_addrs.end(); }
  const_pool_iterator pool_begin() const { return _pool_addrs.begin(); }
  const_pool_iterator pool_end() const { return _pool_addrs.end(); }
  size_t pool_cnt() const { return _pool_addrs.size(); }
  bool pool_empty() const { return _pool_addrs.empty(); }
  TracePoolListTy &pool_addrs() { return _pool_addrs; }

  // Map all addresses in the trace to the corresponding LLVM instructions
  bool addressesToInstructions(matching::Matcher *matcher);
  // Map one address in the trace to the corresponding LLVM instruction
  bool addressToInstruction(PmemAddrTraceItem *item,
                            matching::Matcher *matcher);

  // Try to calculate the pool offsets of a dynamic address
  bool calculatePoolOffsets();

  // Deserialize the address trace from file
  static bool deserialize(const char *fileName, PmemVarGuidMap *varMap,
                          PmemAddrTrace &result, bool ignoreBadLine = false);

 protected:
  TraceListTy _items;
  TracePoolListTy _pool_addrs;

  // Keep a map here to avoid repeated querying the matcher for the same
  // guid. Note that from modularity point of view, we should keep this
  // map in PmemVarGuidMap and fill it as we call PmemVarGuidMap::deserialize.
  // We put it here for now just hope that we don't have to resolve some
  // GUIDs if they don't appear in the trace file...
  std::map<uint64_t, llvm::Instruction *> _guid_instr_map;
  std::map<uint64_t, std::string> _failed_guids;
};

}  // namespace llvm
}  // namespace instrument

#endif /* _INSTRUMENT_PMEMADDRTRACE_H_ */
