// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "Instrument/PmemAddrInstrumenter.h"

using namespace llvm;
using namespace llvm::pmem;
using namespace llvm::instrument;

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
  auto I32Ty = Type::getInt32Ty(llvm_context);
  auto I8PtrTy = Type::getInt8PtrTy(llvm_context);
  auto I32PtrTy = Type::getInt32PtrTy(llvm_context);
  auto VoidTy = Type::getVoidTy(llvm_context);

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
    errs() << "found tracker initialization function " << funcName << "\n";
  }

  funcName = getRuntimeHookName();
  trackAddrFunc = cast<Function>(M.getOrInsertFunction(funcName, VoidTy, 
        I32PtrTy, nullptr));
  if (!trackAddrFunc) {
    errs() << "could not find function " << funcName << "\n";
    return false;
  }
  else {
    errs() << "found track address function " << funcName << "\n";
  }

  funcName = getTrackDumpHookName();
  trackerDumpFunc =
      cast<Function>(M.getOrInsertFunction(funcName, I1Ty, nullptr));
  if (!trackerDumpFunc) {
    errs() << "could not find function " << funcName << "\n";
    return false;
  }
  else {
    errs() << "found track dump function " << funcName << "\n";
  }

  std::vector<Type *> printfArgsTypes({I8PtrTy});
  FunctionType *printfType = FunctionType::get(I32Ty, printfArgsTypes, true);
  printfFunc = cast<Function>(M.getOrInsertFunction("printf", printfType));
  if (!printfFunc) {
    errs() << "could not find printf\n";
    return false;
  }
  else {
    errs() << "found printf\n";
  }
  // insert tracker init func
  IRBuilder<> builder(cast<Instruction>(main->front().getFirstInsertionPt()));
  errs() << "Instrumenting call to " << getRuntimeHookInitName() << " in main\n";
  builder.CreateCall(trackerInitFunc);
  errs() << "Done\n";
  initialized = true;
  return true;
}

bool PmemAddrInstrumenter::instrumentSlice(
    llvm::slicing::DgSlice slice,
    std::map<Value *, Instruction *> pmemMetadata) {
  llvm::Value *v = slice.root_node->getValue();
  if(pmemMetadata.find(v) != pmemMetadata.end()){
     //found element
     instrumentInstr(pmemMetadata.at(v));
  }
  for(auto i = slice.dep_nodes.begin(); i != slice.dep_nodes.end(); ++i){
    //for each node, check if instruction is a persistent value. If it is
    //then get definition point
    dg::LLVMNode *n = *i;
    v = n->getValue();
    if(pmemMetadata.find(v) != pmemMetadata.end()){
       //found element
       instrumentInstr(pmemMetadata.at(v));
    }
  }
  return false;
}

bool PmemAddrInstrumenter::instrumentInstr(Instruction *instr) {
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
    }
    else {
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
  if(pool){
    str = builder.CreateGlobalStringPtr("POOL address: %p\n");
    pool_addr = addr;
  }else{
    str = builder.CreateGlobalStringPtr("address: %p\n");
  }
  // Value *str = builder.CreateGlobalStringPtr("address: %p\n");
  std::vector<llvm::Value *> params;
  params.push_back(str);
  params.push_back(addr);
  CallInst::Create(printfFunc, params, "call", instr->getNextNode());
  return true;
}
