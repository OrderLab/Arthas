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
using namespace llvm::slicing;

unsigned int llvm::instrument::PmemVarGuidStart = 200;
const char *llvm::instrument::PmemVarGuidFileFieldSep = "##";

static unsigned int PmemVarCurrentGuid = llvm::instrument::PmemVarGuidStart;

bool PmemAddrInstrumenter::initHookFuncs(Module &M) {
  if (initialized) {
    errs() << "already initialized\n";
    return true;
  }
  Function *main = M.getFunction("main");
  if (!main) {
    errs() << "Failed to find main function\n";
    return false;
  }

  auto &llvm_context = M.getContext();
  auto I1Ty = Type::getInt1Ty(llvm_context);
  auto VoidTy = Type::getVoidTy(llvm_context);

  I32Ty = Type::getInt32Ty(llvm_context);

  // need i8* for later arthas_track_addr call
  I8PtrTy = Type::getInt8PtrTy(llvm_context);

  // Add the external address tracker function declarations.
  //
  // Here we only need to add the declarations; the definitions of these
  // functions will be provided when linking the instrumented bitcode
  // with the address tracker runtime library.
  //
  // The declaration must match the function signatures defined in 
  // runtime/addr_tracker.h

  StringRef funcName = getRuntimeHookInitName();
  trackerInitFunc = cast<Function>(M.getOrInsertFunction(funcName, VoidTy, nullptr));
  if (!trackerInitFunc) {
    errs() << "could not find function " << funcName << "\n";
    return false;
  }
  else {
    DEBUG(dbgs() << "found tracker initialization function " << funcName << "\n");
  }

  trackAddrFunc = cast<Function>(M.getOrInsertFunction(
      getRuntimeHookName(), VoidTy, I8PtrTy, I32Ty, nullptr));
  if (!trackAddrFunc) {
    errs() << "could not find function " << getRuntimeHookName() << "\n";
    return false;
  } else {
    DEBUG(dbgs() << "found track address function " << getRuntimeHookName() << "\n");
  }

  trackerDumpFunc = cast<Function>(M.getOrInsertFunction(getTrackDumpHookName(), I1Ty, nullptr));
  if (!trackerDumpFunc) {
    errs() << "could not find function " << getTrackDumpHookName() << "\n";
    return false;
  }
  else {
    DEBUG(dbgs() << "found track dump function " << getTrackDumpHookName() << "\n");
  }

  trackerFinishFunc = cast<Function>(M.getOrInsertFunction(getTrackHookFinishName(), VoidTy, nullptr));
  if (!trackerFinishFunc) {
    errs() << "could not find function " << getTrackHookFinishName() << "\n";
    return false;
  } else {
    DEBUG(dbgs() << "found track finish function " << getTrackHookFinishName() << "\n");
  }

  // get or create printf function declaration:
  //    int printf(const char * format, ...);
  vector<Type *> printfArgsTypes;
  printfArgsTypes.push_back(I8PtrTy); 
  FunctionType *printfType = FunctionType::get(I32Ty, printfArgsTypes, true);
  printfFunc = cast<Function>(M.getOrInsertFunction("printf", printfType));
  if (!printfFunc) {
    errs() << "could not find printf\n";
    return false;
  }
  else {
    DEBUG(dbgs() << "found printf\n");
  }

  // insert call to __arthas_addr_tracker_init at the beginning of main function
  IRBuilder<> builder(cast<Instruction>(main->front().getFirstInsertionPt()));
  builder.CreateCall(trackerInitFunc);
  errs() << "Instrumented call to " << getRuntimeHookInitName() << " in main\n";

  // insert call to __arthas_addr_tracker_finish at program exit
  appendToGlobalDtors(M, trackerFinishFunc, 1);
  errs() << "Instrumented call to " << getTrackHookFinishName() << " in main\n";

  initialized = true;
  return true;
}

bool PmemAddrInstrumenter::instrumentSlice(Slice *slice, 
    map<Value *, Instruction *> &pmemMetadata) {
  for (auto i = slice->begin(); i != slice->end(); ++i) {
    // For each node, check if instruction is a persistent value. If it is
    // then get definition point. The root node is also in the iterator (the
    // first one), so we don't need to specially handle it.
    Value *val  = *i;
    auto pmi = pmemMetadata.find(val);
    if (pmi != pmemMetadata.end()) {
      // found element
      instrumentInstr(pmi->second);
    }
  }
  return false;
}

bool PmemAddrInstrumenter::instrumentInstr(Instruction *instr)
{
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
      // FIXME: variable 'addr' is used uninitialized!
      pool = true;
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
    pool_addr = addr;
  } else {
    str = builder.CreateGlobalStringPtr("address: %p\n");
  }
  std::vector<llvm::Value *> params;
  params.push_back(str);
  params.push_back(addr);
  // builder.CreateCall(printfFunc, params);

  // insert an __arthas_track_addr call
  // need to explicitly cast the address, which could be i32* or i64*, to i8*

  hookPointGuidMap[PmemVarCurrentGuid] = instr;
  auto i8addr = builder.CreateBitCast(addr, I8PtrTy);
  auto guid = ConstantInt::get(I32Ty, PmemVarCurrentGuid, false);
  builder.CreateCall(trackAddrFunc, {i8addr, guid});
  PmemVarCurrentGuid++;
  return true;
}

void PmemAddrInstrumenter::dumpHookGuidMapToFile(std::string fileName) 
{
  std::ofstream guidfile(fileName);
  if (!guidfile.is_open()) {
    errs() << "Failed to open " << fileName << " for writing guid map\n";
    return;
  }
  for (auto gi = hookPointGuidMap.begin(); gi != hookPointGuidMap.end(); ++gi) {
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
    guidfile << guid << PmemVarGuidFileFieldSep << SP->getDirectory().data() << PmemVarGuidFileFieldSep;
    guidfile << SP->getFilename().data() << PmemVarGuidFileFieldSep;
    guidfile << func->getName().data() << PmemVarGuidFileFieldSep;
    guidfile << line << PmemVarGuidFileFieldSep << instr_str << "\n";
  }
  guidfile.close();
}
