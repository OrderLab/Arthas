// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Instrument/PmemAddrInstrumenter.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <fstream>
#include <iostream>

#define DEBUG_TYPE "pmem-addr-instrumenter"

using namespace std;
using namespace llvm;
using namespace llvm::pmem;
using namespace llvm::instrument;

unsigned int llvm::instrument::PmemVarGuidStart = 200;
const char *llvm::instrument::PmemVarGuidFileFieldSep = "##";

static unsigned int PmemVarCurrentGuid = llvm::instrument::PmemVarGuidStart;

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
  _tracker_init_func = cast<Function>(M.getOrInsertFunction(funcName, VoidTy, nullptr));
  if (!_tracker_init_func) {
    errs() << "could not find function " << funcName << "\n";
    return false;
  } else {
    DEBUG(dbgs() << "found tracker initialization function " << funcName << "\n");
  }

  _track_addr_func = cast<Function>(M.getOrInsertFunction(
      getRuntimeHookName(), VoidTy, _I8PtrTy, _I32Ty, nullptr));
  if (!_track_addr_func) {
    errs() << "could not find function " << getRuntimeHookName() << "\n";
    return false;
  } else {
    DEBUG(dbgs() << "found track address function " << getRuntimeHookName() << "\n");
  }

  _tracker_dump_func = cast<Function>(
      M.getOrInsertFunction(getTrackDumpHookName(), I1Ty, nullptr));
  if (!_tracker_dump_func) {
    errs() << "could not find function " << getTrackDumpHookName() << "\n";
    return false;
  }
  else {
    DEBUG(dbgs() << "found track dump function " << getTrackDumpHookName() << "\n");
  }

  _tracker_finish_func = cast<Function>(
      M.getOrInsertFunction(getTrackHookFinishName(), VoidTy, nullptr));
  if (!_tracker_finish_func) {
    errs() << "could not find function " << getTrackHookFinishName() << "\n";
    return false;
  } else {
    DEBUG(dbgs() << "found track finish function " << getTrackHookFinishName() << "\n");
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
  }
  else {
    DEBUG(dbgs() << "found printf\n");
  }

  // insert call to __arthas_addr_tracker_init at the beginning of main function
  IRBuilder<> builder(cast<Instruction>(_main->front().getFirstInsertionPt()));
  builder.CreateCall(_tracker_init_func);
  errs() << "Instrumented call to " << getRuntimeHookInitName() << " in main\n";

  // insert call to __arthas_addr_tracker_finish at program exit
  appendToGlobalDtors(M, _tracker_finish_func, 1);
  errs() << "Instrumented call to " << getTrackHookFinishName() << " in main\n";

  _initialized = true;
  return true;
}

bool PmemAddrInstrumenter::instrumentInstr(Instruction *instr) {
  // first check if this instruction has been instrumented before
  if (_hook_point_guid_map.find(instr) != _hook_point_guid_map.end()) {
    errs() << "Skip instrumenting instruction (" << *instr
           << ") as it has been instrumented before\n";
    return false;
  }

  Value *addr;
  bool pool = false;
  if (isa<LoadInst>(instr)) {
    LoadInst *li = dyn_cast<LoadInst>(instr);
    addr = li->getPointerOperand();
  } else if (isa<StoreInst>(instr)) {
    StoreInst *si = dyn_cast<StoreInst>(instr);
    addr = si->getPointerOperand();
  } else if(isa<CallInst>(instr)){
    CallInst *ci = dyn_cast<CallInst>(instr);
    Function *callee = ci->getCalledFunction();
    if(callee->getName().compare("pmemobj_create") == 0){
      pool = true;
      addr = ci;
    } else {
      return false;
    }
  } else {
    return false;
  }

  // TODO: insert a call instruction to the hook function with CallInst::Create.
  // Pass addr as an argument to this call instruction
  IRBuilder <> builder(instr);
  builder.SetInsertPoint(instr->getNextNode());

  // FIXME: use the address tracker runtime lib tracing API instead of printf

  // insert a printf call
  Value *str;
  if (pool) {
    str = builder.CreateGlobalStringPtr("POOL address: %p\n");
  } else {
    str = builder.CreateGlobalStringPtr("address: %p\n");
  }
  std::vector<llvm::Value *> params;
  params.push_back(str);
  params.push_back(addr);
  // builder.CreateCall(printfFunc, params);

  // insert an __arthas_track_addr call
  // need to explicitly cast the address, which could be i32* or i64*, to i8*

  _hook_point_guid_map[instr] = PmemVarCurrentGuid;
  _guid_hook_point_map[PmemVarCurrentGuid] = instr;
  auto i8addr = builder.CreateBitCast(addr, _I8PtrTy);
  auto guid = ConstantInt::get(_I32Ty, PmemVarCurrentGuid, false);
  builder.CreateCall(_track_addr_func, {i8addr, guid});
  PmemVarCurrentGuid++;
  _instrument_cnt++;
  return true;
}

void PmemAddrInstrumenter::writeGuidHookPointMap(std::string fileName)
{
  std::ofstream guidfile(fileName);
  if (!guidfile.is_open()) {
    errs() << "Failed to open " << fileName << " for writing guid map\n";
    return;
  }
  for (auto gi = _guid_hook_point_map.begin(); 
        gi != _guid_hook_point_map.end(); ++gi) {
    unsigned int guid = gi->first;
    Instruction *instr = gi->second;
    auto &Loc = instr->getDebugLoc();
    Function *func = instr->getFunction();
    DISubprogram *SP = func->getSubprogram();
    unsigned int line = 0;
    if (Loc) {
      line = Loc.getLine();
    } else {
      // if this instruction does not have attached metadata, we assume it
      // is the beginning of the function, and use the function line number 
      line = SP->getLine(); 
    }
    std::string instr_str;
    llvm::raw_string_ostream rso(instr_str);
    instr->print(rso);

    // Now output the key information for this instrumented instruction, each
    // piece of information is separated by the field separator. Later we will
    // use this guid information to locate the LLVM instruction for a printed address.
    //
    // The more information we record in this map, the better it helps with 
    // precisely locating the instruction. If, for example, we only record the
    // file name and line number, there could be multiple LLVM instructions.
    guidfile << guid << PmemVarGuidFileFieldSep << SP->getDirectory().data()
             << PmemVarGuidFileFieldSep;
    guidfile << SP->getFilename().data() << PmemVarGuidFileFieldSep;
    guidfile << func->getName().data() << PmemVarGuidFileFieldSep;
    guidfile << line << PmemVarGuidFileFieldSep << instr_str << "\n";
  }
  guidfile.close();
}
