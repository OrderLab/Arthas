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

The persistent memory programs are located in the `test/pmem` directory. Each
program has a corresponding Makefile to build the executable as well as the
bitcode files.

2. Large software

For large software, we need to modify its build system to use clang for compilation.
The most systematic way is using the [WLLVM](https://github.com/travitch/whole-program-llvm) wrapper.

TBA

# Usage

```
opt -load build/analyzer/lib/libLLVMSlicer.so -hello < test/loop1.bc > /dev/null
```

Run our LLVMPMem pass on a persistent memory test program:

```
opt -load build/analyzer/lib/libLLVMPMem.so -pmem < test/pmem/hello_libpmem.bc > /dev/null
```

The output is:
```
[write_hello_string] calling function pmem_map_file
[write_hello_string] calling function perror
[write_hello_string] calling function exit
[write_hello_string] calling function strcpy
[write_hello_string] calling function pmem_persist
[write_hello_string] calling function pmem_msync
[write_hello_string] calling function printf
 Identified pmdk API calls:   %9 = call i8* @pmem_map_file(i8* %8, i64 1024, i32 1, i32 438, i64* %6, i32* %7)
 Identified pmdk API calls:   call void @pmem_persist(i8* %19, i64 %20)
 Identified pmdk API calls:   %24 = call i32 @pmem_msync(i8* %22, i64 %23)
[read_hello_string] calling function pmem_map_file
[read_hello_string] calling function perror
[read_hello_string] calling function exit
[read_hello_string] calling function printf
 Identified pmdk API calls:   %7 = call i8* @pmem_map_file(i8* %6, i64 1024, i32 1, i32 438, i64* %4, i32* %5)
[main] calling function llvm.memcpy.p0i8.p0i8.i64
[main] calling function strcmp
[main] calling function write_hello_string
[main] calling function strcmp
[main] calling function read_hello_string
[main] calling function fprintf
[main] calling function exit
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
