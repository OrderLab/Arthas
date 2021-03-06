# Source code repository for Arthas

Table of Contents
=================

   * [Source code repository for Arthas](#source-code-repository-for-arthas)
      * [Requirements](#requirements)
      * [Environment Variables](#environment-variables)
      * [Build](#build)
         * [Compile Arthas](#compile-arthas)
         * [Compile custom PMDK](#compile-custom-pmdk)
         * [Compile test programs](#compile-test-programs)
         * [Compile a large system for analysis](#compile-a-large-system-for-analysis)
      * [Usage](#usage)
      * [Code Styles](#code-styles)

## Requirements

* Arthas is tested in a Linux machine running Ubuntu 18.04. The server should have 
a persistent memory device (Intel Optane DC persistent memory) or at least 
an [emulated device](https://pmem.io/2016/02/22/pm-emulation.html). 
* The Arthas analyzer is built on top of LLVM 3.9. 
* To analyze a target system using LLVM, we should install the `wllvm` wrapper: `pip install wllvm`.
* We also need to build a custom PMDK (Persistent Memory Development Kit) from its source code, thus [PMDK's dependencies](https://github.com/pmem/pmdk#dependencies) should be installed. 
  * For Ubuntu 18.04, the installation command is:
```
$ sudo apt install autoconf automake pkg-config libglib2.0-dev libfabric-dev pandoc libncurses5-dev
```

## Environment Variables

To use the `wllvm` wrapper for compiling a target system, set the following 
environment variables:

``` 
export LLVM_COMPILER=clang
export LLVM_HOME=/opt/software/llvm/3.9.1/dist
export LLVM_COMPILER_PATH=$LLVM_HOME/bin
export PATH=$LLVM_COMPILER_PATH:$PATH
```

The `LLVM_HOME` path should be replaced appropriately if running on a different machine.

## Build

### Compile Arthas

```
$ git clone git@github.com:OrderLab/Arthas.git
$ git submodule update --init
$ mkdir build
$ cd build && cmake ..
$ make -j4
```

### Compile custom PMDK

```
$ cd ../pmdk
$ make -j $(nproc)
```

### Compile test programs

We include a set of simple test programs for testing the Arthas analyzer,
detector, and reactor during development.

For them to be used by the analyzer, we need to compile these test programs
into bitcode files using `clang`, e.g.,

```
clang -c -emit-llvm test/loop1.c -o test/loop1.bc
```

For convenience, you can compile the bitcode files for all test programs with:
```
cd test
make
```

The persistent memory programs are located in the `test/pmem` directory. Each
program has a corresponding Makefile to build the executable as well as the
bitcode files.


### Compile a large system for analysis

For large software, we need to modify its build system to use clang for compilation.
The most systematic way is using the [WLLVM](https://github.com/travitch/whole-program-llvm) 
wrapper.

Below is an example of compiling Memcached (a buggy version) for analysis:

```
$ mkdir -p eval-sys
$ git clone https://github.com/OrderLab/Arthas-eval-Memcached eval-sys/memcached
$ cd eval-sys/memcached
$ git checkout refcount
$ ./autogen.sh
$ CC=wllvm CFLAGS="-g -O0" LDFLAGS="-Wl,--no-as-needed -ldl" ./configure --enable-pslab
$ ln -s ../../vanilla-pmdk pmdk
$ make -j$(nproc)
$ extract-bc memcached
```

If successful, a bitcode file `memcached.bc` will be extracted.

## Usage

Arthas consists of the analyzer, detector and reactor components. The usage
instruction for each component is in the README of its corresponding directory.

## Code Styles

Please refer to the [code style](codestyle.md) for the coding convention and practice.
