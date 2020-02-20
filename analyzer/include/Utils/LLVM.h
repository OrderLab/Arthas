// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#ifndef _UTILS_LLVM_H_
#define _UTILS_LLVM_H_

#include <string>
#include <memory>

#include <llvm/Config/llvm-config.h>

#if LLVM_VERSION_MAJOR >= 4
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#else
#include <llvm/Bitcode/ReaderWriter.h>
#endif

#include <llvm/IR/Instructions.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_os_ostream.h>

std::unique_ptr<llvm::Module> parseModule(llvm::LLVMContext& context, 
    std::string inputFile);

#endif /* _UTILS_LLVM_H_ */
