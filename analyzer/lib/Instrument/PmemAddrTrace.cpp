// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Instrument/PmemAddrTrace.h"
#include "Instrument/PmemVarGuidMap.h"
#include "Utils/String.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;
using namespace llvm;
using namespace llvm::instrument;

// Must be consistent with the field separator used in the address tracker lib
const char *PmemAddrTrace::FieldSeparator = ",";

// the pmemobj_create call instruction string to identify pool addresses
static const char *PmemObjCreateCallInstrStr =
    "call %struct.pmemobjpool* @pmemobj_create";

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
    PmemAddrTraceItem item;
    item.addr_str = parts[0];
    // convert the hex address into a decimal uint64 value
    if (item.addr_str[0] == '0' &&
        (item.addr_str[1] == 'x' || item.addr_str[1] == 'X')) {
      // remove the 0x or 0X prefix in the address string
      item.addr = str2fmt<uint64_t>(item.addr_str.substr(2), true);
    } else {
      // the address string is not prefixed with 0x or 0X
      item.addr = str2fmt<uint64_t>(item.addr_str, true);
    }
    item.guid = str2fmt<uint64_t>(parts[1]);
    if (varMap != nullptr) {
      // if the GUID map is supplied, we'll resolve the corresponding pmem
      // variable information from the map with the GUID
      auto vi = varMap->find(item.guid);
      if (vi != varMap->end()) {
        item.var = &vi->second;
        // FIXME: for now we identify whether a trace item is from a pool
        // address
        // by checking the associated instruction string in the map.
        // Another way is to directly record a flag in the trace entry to
        // indicate whether the address is a pool address. This would
        // require modifying the instrumenter and address tracker lib API.
        if (item.var->instruction.find(PmemObjCreateCallInstrStr) !=
            string::npos) {
          errs() << "Found a pool address " << item.addr_str << "\n";
          item.is_pool = true;
        }
      }
    }
    result.add(item);
  }
  addrfile.close();
  return true;
}
