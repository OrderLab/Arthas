// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef __INSTRUMENT_PMEMADDR_H_
#define __INSTRUMENT_PMEMADDR_H_

#include "Instrument/PmemVarGuidMap.h"
#include "PMem/Extractor.h"

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
#include <set>
#include <string>

namespace llvm {

namespace slicing {
class Slice;
}

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

inline StringRef getLowLevelInitName() {
  return "__arthas_low_level_init";
}

inline StringRef getLowLevelFlushName() {
  return "__arthas_low_level_flush";
}

inline StringRef getLowLevelFenceName() {
  return "__arthas_low_level_fence";
}

inline StringRef getSaveFileName() {
  return "__arthas_save_file";
}

class PmemAddrInstrumenter {
 public:
  // by default we will use our lightweight runtime library for tracking
  // setting use_printf to true will use printf for tracking
  PmemAddrInstrumenter(bool use_printf = false)
      : _initialized(false), _instrument_cnt(0),
        _track_with_printf(use_printf) {}

  bool initHookFuncs(Module &M);

  // instrument a call to hook func before an instruction.
  // this instruction must be a LoadInst or StoreInst
  bool instrumentInstr(Instruction *instr);

  bool instrumentSlice(
      slicing::Slice *slice,
      std::map<llvm::Instruction *, std::set<llvm::Value *>> &useDefMap);

  // dump the guid to instruction map to file so that we can later connect the
  // address back to the LLVM instruction
  bool writeGuidHookPointMap(std::string fileName);

  uint32_t getInstrumentedCnt() { return _instrument_cnt; }

  static bool fillVarGuidMapInfo(llvm::Instruction *instr,
                                 PmemVarGuidMapEntry &entry);

 protected:
  bool _initialized;
  uint32_t _instrument_cnt;
  // instrument printf to track addresses, slow but useful for testing
  bool _track_with_printf;

  Function *_main;
  Function *_track_addr_func;
  Function *_tracker_init_func;
  Function *_tracker_dump_func;
  Function *_tracker_finish_func;
  Function *_printf_func;
  Function *_low_level_init_func;
  Function *_low_level_flush_func;
  Function *_low_level_fence_func;
  Function *_save_file_func;

  std::map<uint64_t, Instruction *> _guid_hook_point_map;
  std::map<Instruction *, uint64_t> _hook_point_guid_map;

  IntegerType *_I32Ty;
  PointerType *_I8PtrTy;
};

}  // namespace llvm
}  // namespace instrument

#endif /* __INSTRUMENT_PMEMADDR_H_ */
