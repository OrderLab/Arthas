// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Instrument/PmemAddrInstrumenter.h"
#include "Slicing/Slice.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugLoc.h>

#include <unistd.h>
#include <fstream>
#include <iostream>

#define DEBUG_TYPE "pmem-addr-instrumenter"
//#define MAX_ADDRESSES 1000005
using namespace std;
using namespace llvm;
using namespace llvm::pmem;
using namespace llvm::slicing;
using namespace llvm::instrument;

unsigned int llvm::instrument::PmemVarGuidStart = 200;
const char *llvm::instrument::PmemVarGuidFileFieldSep = "##";

static unsigned int PmemVarCurrentGuid = llvm::instrument::PmemVarGuidStart;

// for nested dereferencing
multimap<int, int> line_no_consec_instr;
multimap<int, Instruction *> line_no_instructions;
int prev_line_no;
int consec_count = 0;
unsigned prev_opcode;

// llvm::Value * addresses[MAX_ADDRESSES];
// llvm::Value *guids[MAX_ADDRESSES];
int address_count = 0;
int instr_count = 0;
// char *addresses[MAX_ADDRESSES];
// int guids[MAX_ADDRESSES];

bool PmemAddrInstrumenter::initHookFuncs(Module &M) {
  if (_initialized) {
    errs() << "already initialized\n";
    return true;
  }
  _main = M.getFunction("main");
  if (!_main) {
    errs() << "Failed to find main function\n";
    return false;
  }

  auto &llvm_context = M.getContext();
  auto I1Ty = Type::getInt1Ty(llvm_context);
  auto VoidTy = Type::getVoidTy(llvm_context);

  _I32Ty = Type::getInt32Ty(llvm_context);

  // need i8* for later arthas_track_addr call
  _I8PtrTy = Type::getInt8PtrTy(llvm_context);

  // Add the external address tracker function declarations.
  //
  // Here we only need to add the declarations; the definitions of these
  // functions will be provided when linking the instrumented bitcode
  // with the address tracker runtime library.
  //
  // The declaration must match the function signatures defined in
  // runtime/addr_tracker.h

  StringRef funcName = getRuntimeHookInitName();
  _tracker_init_func =
      cast<Function>(M.getOrInsertFunction(funcName, VoidTy, nullptr));
  if (!_tracker_init_func) {
    errs() << "could not find function " << funcName << "\n";
    return false;
  } else {
    DEBUG(dbgs() << "found tracker initialization function " << funcName
                 << "\n");
  }

  // Create Function for low level init
  _low_level_init_func = cast<Function>(
      M.getOrInsertFunction(getLowLevelInitName(), VoidTy, nullptr));
  if (!_low_level_init_func) {
    errs() << "could not find function " << getLowLevelInitName() << "\n";
    return false;
  }

  // Create Function for save file
  _save_file_func = cast<Function>(
      M.getOrInsertFunction(getSaveFileName(), VoidTy, _I8PtrTy, nullptr));
  if (!_save_file_func) {
    errs() << "could not find function " << getSaveFileName() << "\n";
    return false;
  }

  // Create Function for low level fence
  _low_level_fence_func = cast<Function>(
      M.getOrInsertFunction(getLowLevelFenceName(), VoidTy, nullptr));
  if (!_low_level_fence_func) {
    errs() << "could not find function " << getLowLevelFenceName() << "\n";
    return false;
  }

  // Create Function for low level flush
  _low_level_flush_func = cast<Function>(
      M.getOrInsertFunction(getLowLevelFlushName(), VoidTy, nullptr));
  if (!_low_level_flush_func) {
    errs() << "could not find function " << getLowLevelFlushName() << "\n";
    return false;
  }

  _track_addr_func = cast<Function>(M.getOrInsertFunction(
      getRuntimeHookName(), VoidTy, _I8PtrTy, _I32Ty, nullptr));
  if (!_track_addr_func) {
    errs() << "could not find function " << getRuntimeHookName() << "\n";
    return false;
  } else {
    DEBUG(dbgs() << "found track address function " << getRuntimeHookName()
                 << "\n");
  }

  _tracker_dump_func = cast<Function>(
      M.getOrInsertFunction(getTrackDumpHookName(), I1Ty, nullptr));
  if (!_tracker_dump_func) {
    errs() << "could not find function " << getTrackDumpHookName() << "\n";
    return false;
  } else {
    DEBUG(dbgs() << "found track dump function " << getTrackDumpHookName()
                 << "\n");
  }

  _tracker_finish_func = cast<Function>(
      M.getOrInsertFunction(getTrackHookFinishName(), VoidTy, nullptr));
  if (!_tracker_finish_func) {
    errs() << "could not find function " << getTrackHookFinishName() << "\n";
    return false;
  } else {
    DEBUG(dbgs() << "found track finish function " << getTrackHookFinishName()
                 << "\n");
  }

  // get or create printf function declaration:
  //    int printf(const char * format, ...);
  vector<Type *> printfArgsTypes;
  printfArgsTypes.push_back(_I8PtrTy);
  FunctionType *printfType = FunctionType::get(_I32Ty, printfArgsTypes, true);
  _printf_func = cast<Function>(M.getOrInsertFunction("printf", printfType));
  if (!_printf_func) {
    errs() << "could not find printf\n";
    return false;
  } else {
    DEBUG(dbgs() << "found printf\n");
  }

  // Only insert the tracker init and finish function if we choose to instrument
  // using the runtime library. For printf-based tracking, it is not needed.
  if (!_track_with_printf) {
    // insert call to __arthas_addr_tracker_init at the beginning of main
    // function
    IRBuilder<> builder(
        cast<Instruction>(_main->front().getFirstInsertionPt()));
    builder.CreateCall(_tracker_init_func);
    errs() << "Instrumented call to " << getRuntimeHookInitName()
           << " in main\n";

    // insert call to __arthas_low_level_init() at beginning of main
    builder.CreateCall(_low_level_init_func);
    errs() << "Instrumented call to " << getLowLevelInitName() << " in main\n";

    // insert call to __arthas_addr_tracker_finish at program exit
    appendToGlobalDtors(M, _tracker_finish_func, 1);
    errs() << "Instrumented call to " << getTrackHookFinishName()
           << " in main\n";
  }

  _initialized = true;
  return true;
}

bool PmemAddrInstrumenter::instrumentInstr(Instruction *instr) {
  // return false;
  // first check if this instruction has been instrumented before
  if (_hook_point_guid_map.find(instr) != _hook_point_guid_map.end()) {
    DEBUG(errs() << "Skip instrumenting instruction (" << *instr
                 << ") as it has been instrumented before\n");
    return false;
  }
  Value *addr;
  bool pool = false;
  bool malloc_flag = false;
  bool flush_flag = false;
  bool fence_flag = false;

  if (isa<LoadInst>(instr)) {
    LoadInst *li = dyn_cast<LoadInst>(instr);
    addr = li->getPointerOperand();
  } else if (isa<StoreInst>(instr)) {
    StoreInst *si = dyn_cast<StoreInst>(instr);
    addr = si->getPointerOperand();
  } else if (isa<GetElementPtrInst>(instr)) {
    GetElementPtrInst *gepInst = cast<GetElementPtrInst>(instr);
    addr = gepInst->getPointerOperand();
    //return false;
  } else if (isa<CallInst>(instr)) {
    CallInst *ci = dyn_cast<CallInst>(instr);
    Function *callee = ci->getCalledFunction();
    if(!callee){
      return false;
    }
    istringstream iss(callee->getName());
    std::string token;
    std::getline(iss, token, '.');
    if (token.compare("llvm") == 0) {
      for (int i = 0; i < 3; i++) {
        std::getline(iss, token, '.');
      }
    }
    if (token.compare("pmemobj_create") == 0) {
      pool = true;
      addr = ci;
    } else if (PMemVariableLocator::callReturnsPmemVar(token.data())) {
      addr = ci;
    } else if (token.compare("pmemobj_tx_add_range_direct") == 0) {
      addr = ci->getOperand(0);
    } else if (token.compare("pmemobj_direct_inline") == 0) {
      addr = ci->getOperand(0);
    } else if (token.compare("pmemobj_persist") == 0) {
      addr = ci->getOperand(1);
    } else if (token.compare("sfence") == 0) {
      // sfence function
      fence_flag = true;
      // return true;
    } else if (token.compare("clflush") == 0) {
      // cflush function
      addr = ci->getOperand(0);
      errs() << "deref address is " << *addr << "\n";
      flush_flag = true;
    } else if (token.compare("mmap") == 0) {
      addr = ci;
      malloc_flag = true;
    } else {
      errs() << "return false\n";
      return false;
    }
  } else {
    errs() << "return false\n";
    return false;
  }

  // TODO: insert a call instruction to the hook function with CallInst::Create.
  // Pass addr as an argument to this call instruction
  IRBuilder<> builder(instr);
  builder.SetInsertPoint(instr->getNextNode());

  // Low Level Support
  /*if(malloc_flag){
    auto i8addr = builder.CreateBitCast(addr, _I8PtrTy);
    builder.CreateCall(_save_file_func, {i8addr});
    _instrument_cnt++;
    return true;
  }
  if(fence_flag){
    builder.CreateCall(_low_level_fence_func, None);
    _instrument_cnt++;
    return true;
  }
  if(flush_flag){
    auto i8addr = builder.CreateBitCast(addr, _I8PtrTy);
    builder.CreateCall(_low_level_flush_func, {i8addr});
    _instrument_cnt++;
    return true;
  }
  else if (!flush_flag && !fence_flag){
    // Get rid of this eventually
    return true;
  }*/

  if (_track_with_printf) {
    Value *str;
    if (pool) {
      str = builder.CreateGlobalStringPtr("POOL address: %p\n");
    } else {
      str = builder.CreateGlobalStringPtr("address: %p\n");
    }
    std::vector<llvm::Value *> params;
    params.push_back(str);
    params.push_back(addr);
    builder.CreateCall(_printf_func, params);
  } else {
    // insert an __arthas_track_addr call
    // need to explicitly cast the address, which could be i32* or i64*, to i8*
    _hook_point_guid_map[instr] = PmemVarCurrentGuid;
    _guid_hook_point_map[PmemVarCurrentGuid] = instr;
    auto i8addr = builder.CreateBitCast(addr, _I8PtrTy);
    auto guid = ConstantInt::get(_I32Ty, PmemVarCurrentGuid, false);
    builder.CreateCall(_track_addr_func, {i8addr, guid});
    PmemVarCurrentGuid++;
  }
  _instrument_cnt++;
  return true;
}

bool PmemAddrInstrumenter::instrumentSlice(
    Slice *slice, map<Instruction *, set<Value *>> &useDefMap) {
  bool instrumented = false;
  for (auto i = slice->begin(); i != slice->end(); ++i) {
    // For each node, check if instruction is a persistent value. If it is
    // then get definition point. The root node is also in the iterator (the
    // first one), so we don't need to specially handle it.
    if (Instruction *inst = dyn_cast<Instruction>(i->first)) {
      auto pmi = useDefMap.find(inst);
      if (pmi != useDefMap.end()) {
        DEBUG(errs() << "Found definition point for " << *inst << ":\n");
        for (Value *val : pmi->second) {
          DEBUG(errs() << "---->" << *val << "\n");
          if (Instruction *def_inst = dyn_cast<Instruction>(val)) {
            instrumented |= instrumentInstr(def_inst);
          }
        }
      } else {
        DEBUG(errs() << "Cannot find definition point for " << *inst << "\n");
      }
    }
  }
  return instrumented;
}

// Fill the key information about an instrumented instruction. Each
// piece of information is separated by the field separator. Later we will
// use this guid information to locate the LLVM instruction for a printed
// address.
//
// The more information we record in this map, the better it helps with
// precisely locating the instruction. If, for example, we only record the
// file name and line number, there could be multiple LLVM instructions.
bool PmemAddrInstrumenter::fillVarGuidMapInfo(llvm::Instruction *instr,
                                              PmemVarGuidMapEntry &entry) {
  auto &Loc = instr->getDebugLoc();
  Function *func = instr->getFunction();
  DISubprogram *SP = func->getSubprogram();
  unsigned int line = 0;
  if (Loc) {
    line = Loc.getLine();
  } else {
    // if this instruction does not have attached metadata, we assume it
    // is the beginning of the function, and use the function line number
    if (!SP) {
      printf("UH OH sp is not good\n");
      return true;
    }
    line = SP->getLine();
  }
  std::string instr_str;
  llvm::raw_string_ostream rso(instr_str);
  instr->print(rso);
  entry.source_path = SP->getDirectory().data();
  entry.source_file = SP->getFilename().data();
  entry.function = func->getName().data();
  entry.line = line;
  entry.instruction = instr_str;
  return true;
}

bool PmemAddrInstrumenter::writeGuidHookPointMap(std::string fileName) {
  PmemVarGuidMap var_map;
  for (auto gi = _guid_hook_point_map.begin(); gi != _guid_hook_point_map.end();
       ++gi) {
    PmemVarGuidMapEntry entry;
    entry.guid = gi->first;
    Instruction *instr = gi->second;
    fillVarGuidMapInfo(instr, entry);
    var_map.add(entry);
  }
  return var_map.serialize(fileName.c_str());
}
