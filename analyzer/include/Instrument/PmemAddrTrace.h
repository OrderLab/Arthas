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

#include <string>
#include <vector>

namespace llvm {
namespace instrument {

class PmemVarGuidMap;
class PmemVarGuidMapEntry;

class PmemAddrTraceItem {
 public:
  std::string addr_str;
  uint64_t guid;
  // convert the hex address string into a decimal uint64 address.
  // but we don't seem to really need the int value of address at this point
  uint64_t addr;
  PmemVarGuidMapEntry *var;

  PmemAddrTraceItem() : var(nullptr) {}
};

class PmemAddrTrace {
 public:
  typedef std::vector<PmemAddrTraceItem> TraceListTy;
  typedef TraceListTy::iterator iterator;
  typedef TraceListTy::const_iterator const_iterator;

  static const char *FieldSeparator;
  // should be consistent with address tracker runtime lib
  static const int EntryFields = 2;

 public:
  void add(PmemAddrTraceItem &item) { _items.push_back(item); }
  iterator begin() { return _items.begin(); }
  iterator end() { return _items.end(); }
  const_iterator begin() const { return _items.begin(); }
  const_iterator end() const { return _items.end(); }
  size_t size() const { return _items.size(); }
  TraceListTy &items() { return _items; }

  static bool deserialize(const char *fileName, PmemVarGuidMap *varMap,
                          PmemAddrTrace &result, bool ignoreBadLine = false);

 protected:
  std::vector<PmemAddrTraceItem> _items;
};

}  // namespace llvm
}  // namespace instrument

#endif /* _INSTRUMENT_PMEMADDRTRACE_H_ */
