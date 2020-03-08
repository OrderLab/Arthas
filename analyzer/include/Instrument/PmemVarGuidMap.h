// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _INSTRUMENT_PMEMVARGUIDMAP_H_
#define _INSTRUMENT_PMEMVARGUIDMAP_H_

#include <string>
#include <map>

namespace llvm {
namespace instrument {

typedef uint64_t VarGuidTy;

class PmemVarGuidMapEntry {
 public:
  VarGuidTy guid;
  std::string source_path;
  std::string source_file;
  std::string function;
  uint32_t line;
  std::string instruction;

  PmemVarGuidMapEntry() {}

  PmemVarGuidMapEntry(VarGuidTy var_guid, const char *var_source_path,
                      const char *var_source_file, const char *var_function,
                      uint32_t var_line, const char *var_inst)
      : guid(var_guid), source_path(var_source_path),
        source_file(var_source_file), function(var_function), line(var_line),
        instruction(var_inst) {}
};

class PmemVarGuidMap {
 public:
  typedef std::map<VarGuidTy, PmemVarGuidMapEntry> VarGuidMapTy;
  typedef VarGuidMapTy::iterator iterator;
  typedef VarGuidMapTy::const_iterator const_iterator;

  static const char *FieldSeparator;
  // should be consistent with PmemVarGuidMapEntry definition
  static const int EntryFields = 6;

 public:
  PmemVarGuidMap() {}

  iterator begin() { return _guidMap.begin(); }
  iterator end() { return _guidMap.end(); }

  const_iterator begin() const { return _guidMap.begin(); }
  const_iterator end() const { return _guidMap.end(); }
  size_t size() const { return _guidMap.size(); }

  iterator find(VarGuidTy guid) { return _guidMap.find(guid); }

  void add(PmemVarGuidMapEntry entry) { _guidMap.emplace(entry.guid, entry); }

  VarGuidMapTy &get() { return _guidMap; }

  bool serialize(const char *fileName);

  static bool deserialize(const char *fileName, PmemVarGuidMap &result,
                          bool ignoreBadLine = false);

 protected:
  VarGuidMapTy _guidMap;
};

}  // namespace llvm
}  // namespace instrument

#endif /* _INSTRUMENT_PMEMVARGUIDMAP_H_ */
