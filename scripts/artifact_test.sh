#!/bin/bash

root_dir=$(cd "$(dirname "${BASH_SOURCE-$0}")"/..; pwd)

export LLVM_COMPILER=clang
export LLVM_HOME=/opt/software/llvm/3.9.1/dist
export LLVM_COMPILER_PATH=$LLVM_HOME/bin
export PATH=$LLVM_COMPILER_PATH:$PATH
export PMDK_HOME=$root_dir/vanilla-pmdk

cd $root_dir
git submodule update --init

# build vanilla pmdk first
cd vanilla-pmdk
git checkout tags/1.4.2
make -j$(nproc)
if [ $? -ne 0 ]; then
  echo "Failed to build vanilla PMDK"
  exit 1
fi
echo "Successfully build vanilla PMDK"

# build arthas-pmdk first
cd pmdk
make -j$(nproc)
if [ $? -ne 0 ]; then
  echo "Failed to build Arthas-PMDK"
  exit 1
fi
echo "Successfully build Arthas-PMDK"

export PMDK_HOME=$root_dir/pmdk
# build Arthas
cd $root_dir
mkdir -p build
cd build && cmake ..
make -j$(nproc)
if [ $? -ne 0 ]; then
  echo "Failed to build Arthas"
  exit 1
fi
echo "Successfully build Arthas"

export PMDK_HOME=$root_dir/vanilla-pmdk
# build target system with LLVM, using Memcached as an example
cd $root_dir
mkdir -p eval-sys
if [ ! -d eval-sys/memcached ]; then
  git clone https://github.com/OrderLab/Arthas-eval-Memcached eval-sys/memcached
fi
cd eval-sys/memcached
git checkout refcount
./autogen.sh
CC=wllvm CFLAGS="-g -O0" LDFLAGS="-Wl,--no-as-needed -ldl" ./configure --enable-pslab
if [ ! -L pmdk ]; then
  ln -s $PMDK_HOME 
fi
make -j$(nproc)
if [ $? -ne 0 ]; then
  echo "Failed to build Memcached with LLVM"
  exit 1
fi
echo "Successfully build Memached with LLVM"
extract-bc memcached
if [ ! -f memcached.bc ]; then
  echo "Failed to extract bitcode file from Memcached executable"
  exit 1
fi
echo "Successfully extracted memcached.bc"

# now we have the memcached.bc file, we can initiate the Arthas workflow
export PMDK_HOME=$root_dir/pmdk

cd $root_dir
mkdir -p experiment/memcached
cd experiment/memcached
bc_path=$root_dir/eval-sys/memcached/memcached.bc
link_flags="-Wl,-rpath=${PMDK_HOME} -lpmemobj -lpthread -levent -levent_core"
$root_dir/scripts/instrument-compile.sh -l "$link_flags" --output $root_dir/build/memcached-instrumented $bc_path
if [ $? -ne 0 ]; then
  echo "Failed to instrument Memcached with Arthas"
fi
echo "Successfully instrumented Memached with Arthas"
