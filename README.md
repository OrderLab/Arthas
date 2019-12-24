# Source code repository for PMEM-Fault project

# Build

## Compile the tool

```
mkdir build
cd build && cmake ..
make -j4
```

## Compile a target system for analysis

1. Simple program

For simple program, e.g., the test case in `test` directory, it is sufficient to
use `clang`. For example,

```
clang -c -emit-llvm test/loop1.c -o test/loop1.bc
```

For convenience, you can compile the bitcode files for all test programs with:
```
cd test
make
```

2. Large software

For large software, we need to modify its build system to use clang for compilation.
The most systematic way is using the [WLLVM](https://github.com/travitch/whole-program-llvm) wrapper.

TBA

# Usage

```
opt -load build/analyzer/lib/libLLVMSlicer.so -hello < test/loop1.bc > /dev/null
```

TBA

# Code Style
We follow the [Google Cpp Style Guide](https://google.github.io/styleguide/cppguide.html#Formatting). 
There is a [.clang-format](.clang-format) file in the root directory that is derived from this style.
It can be used with the `clang-format` tool to reformat the source files, e.g.,

```
$ clang-format -style=file analyzer/lib/Slicing/Slicer.cpp
```

This will use the `.clang-format` file to re-format the source file and print it to the console. 
To re-format the file in-place, add the `-i` flag.

```
$ clang-format -i -style=file analyzer/lib/Slicing/Slicer.cpp
$ clang-format -i -style=file analyzer/lib/*/*.cpp
```

If you are using Clion, the IDE supports `.clang-format` style. Go to `Settings/Preferences | Editor | Code Style`, 
check the box `Enable ClangFormat with clangd server`. `clang-format` can also be integrated with 
vim, see the official [doc](http://clang.llvm.org/docs/ClangFormat.html#clion-integration).
