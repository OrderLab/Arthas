// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef __INSTRUMENT_PMEMADDR_H_
#define __INSTRUMENT_PMEMADDR_H_

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/BasicBlock.h"
#include "Slicing/Slicer.h"
#include "Slicing/Slice.h"
#include "PMem/Extractor.h"

namespace llvm {
namespace instrument {

inline StringRef getRuntimeHookInitName() {
  return "__arthas_addr_tracker_init";
}

inline StringRef getRuntimeHookName() {
  return "__arthas_track_addr";
}

inline StringRef getTrackDumpHookName() {
  return "__arthas_addr_tracker_dump";
}

class PmemAddrInstrumenter {
 public:
  PmemAddrInstrumenter() : initialized(false) {}

  bool initHookFuncs(Module &M);

  // instrument the persistent points in a slice
  bool instrumentSlice(llvm::slicing::DgSlice slice,
                  std::map<Value *, Instruction *> pmemMetadata);

  // instrument a call to hook func before an instruction.
  // this instruction must be a LoadInst or StoreInst
  bool instrumentInstr(Instruction * instr);

 protected:
  bool initialized;

  Function *main;
  Function *trackAddrFunc;
  Function *trackerInitFunc;
  Function *trackerDumpFunc;
  Function *printfFunc;

  Value *pool_addr;
  int count;
};

} // namespace llvm
} // namespace instrument

#endif /* __INSTRUMENT_PMEMADDR_H_ */
