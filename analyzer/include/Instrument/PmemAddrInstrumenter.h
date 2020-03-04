// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef __INSTRUMENT_PMEMADDR_H_
#define __INSTRUMENT_PMEMADDR_H_

#include "PMem/Extractor.h"
#include "Slicing/Slice.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Instrumentation.h"

#include <map>
#include <string>

namespace llvm {
namespace instrument {

extern unsigned int PmemVarGuidStart;
extern const char *PmemVarGuidFileFieldSep;

inline StringRef getRuntimeHookInitName() {
  return "__arthas_addr_tracker_init";
}

inline StringRef getRuntimeHookName() {
  return "__arthas_track_addr";
}

inline StringRef getTrackDumpHookName() {
  return "__arthas_addr_tracker_dump";
}

inline StringRef getTrackHookFinishName() {
  return "__arthas_addr_tracker_finish";
}

class PmemAddrInstrumenter {
 public:
  PmemAddrInstrumenter() : initialized(false) {}

  bool initHookFuncs(Module &M);

  // instrument the persistent points in a slice
  bool instrumentSlice(llvm::slicing::Slice *slice,
                       std::map<Value *, Instruction *> &pmemMetadata);

  // instrument a call to hook func before an instruction.
  // this instruction must be a LoadInst or StoreInst
  bool instrumentInstr(Instruction * instr);

  // dump the guid to instruction map to file so that we can later connect the
  // address back to the LLVM instruction
  void dumpHookGuidMapToFile(std::string fileName);

 protected:
  bool initialized;

  Function *main;

  Function *trackAddrFunc;
  Function *trackerInitFunc;
  Function *trackerDumpFunc;
  Function *trackerFinishFunc;

  Function *printfFunc;

  IntegerType *I32Ty;
  PointerType *I8PtrTy;
  Value *pool_addr;
  int count;

  std::map<unsigned int, Instruction *> hookPointGuidMap;
};

} // namespace llvm
} // namespace instrument

#endif /* __INSTRUMENT_PMEMADDR_H_ */
