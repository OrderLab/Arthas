// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Instrument/PmemVarGuidMap.h"
#include "Utils/String.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;
using namespace llvm;
using namespace llvm::instrument;

const char *PmemVarGuidMap::FieldSeparator = "##";

bool PmemVarGuidMap::serialize(const char *fileName) {
  std::ofstream guidfile(fileName);
  if (!guidfile.is_open()) {
    errs() << "Failed to open " << fileName << " for writing guid map\n";
    return false;
  }
  // We could serialize the map into binary format, but for simplicity
  // just write it as textual format with each entry in one line.
  for (auto I = _guidMap.begin(); I != _guidMap.end(); ++I) {
    PmemVarGuidMapEntry &entry = I->second;
    guidfile << entry.guid << FieldSeparator << entry.source_path
             << FieldSeparator;
    guidfile << entry.source_file << FieldSeparator;
    guidfile << entry.function << FieldSeparator;
    guidfile << entry.line << FieldSeparator << entry.instruction << "\n";
  }
  guidfile.close();
  return true;
}

bool PmemVarGuidMap::deserialize(const char *fileName, PmemVarGuidMap &result,
                                 bool ignoreBadLine) {
  std::ifstream guidfile(fileName);
  if (!guidfile.is_open()) {
    errs() << "Failed to open " << fileName << " for reading guid map\n";
    return false;
  }
  string line;
  PmemVarGuidMap guidMap;
  while (getline(guidfile, line)) {
    vector<string> parts;
    splitList(line, FieldSeparator, parts);
    if (parts.size() != EntryFields) {
      errs() << "Unrecognized line " << line << "\n";
      if (!ignoreBadLine) return false;
      continue;
    }
    PmemVarGuidMapEntry entry;
    entry.guid = str2fmt<uint64_t>(parts[0]);
    entry.source_path = parts[1];
    entry.source_file = parts[2];
    entry.function = parts[3];
    entry.line = str2fmt<uint32_t>(parts[4]);
    entry.instruction = parts[5];
    guidMap.add(entry);
  }
  guidfile.close();
  return true;
}
