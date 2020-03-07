# Source code repository for Arthas

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

## Extract PM variables and their slices
Run our LLVMPMem pass on a persistent memory test program:

```
opt -load build/analyzer/lib/libLLVMPMem.so -pmem < test/pmem/hello_libpmem.bc > /dev/null
```

The output is:
```
[write_hello_string]
- this instruction creates a pmem variable:   %9 = call i8* @pmem_map_file(i8* %8, i64 1024, i32 1, i32 438, i64* %6, i32* %7)
- users of the pmem variable:   %13 = load i8*, i8** %5, align 8
- users of the pmem variable:   %15 = call i8* @strcpy(i8* %13, i8* %14) #7
- users of the pmem variable:   %19 = load i8*, i8** %5, align 8
- users of the pmem variable:   call void @pmem_persist(i8* %19, i64 %20)
...
- users of the pmem variable:   br i1 %10, label %11, label %12
* Identified pmdk API calls:   %9 = call i8* @pmem_map_file(i8* %8, i64 1024, i32 1, i32 438, i64* %6, i32* %7)
* Identified pmdk API calls:   call void @pmem_persist(i8* %19, i64 %20)
* Identified pmdk API calls:   %24 = call i32 @pmem_msync(i8* %22, i64 %23)
* Identified pmem variable instruction:   %9 = call i8* @pmem_map_file(i8* %8, i64 1024, i32 1, i32 438, i64* %6, i32* %7)
* Identified pmem variable instruction:   %13 = load i8*, i8** %5, align 8
...
...
```

You can also specify a target function to extract the pmem variables within 
that function.

```
$ opt -load analyzer/lib/libLLVMPMem.so -pmem -target-functions write_vector < ../test/pmem/pmem_vector.bc > /dev/null
```

## Locate LLVM instruction given file & line number

```
$ cd build
$ bin/locator ../test/loop1.bc loop1.c:9,loop1.c:24
Successfully parsed ../test/loop1.bc
Matched function <foo>()@test/loop1.c:3,17
- matched instruction:   %22 = load i32, i32* %5, align 4, !dbg !45
- matched instruction:   %23 = load i32, i32* %3, align 4, !dbg !46
...
Matched function <add>()@test/loop1.c:19,29
- matched instruction:   %10 = load i32, i32* %2, align 4, !dbg !25
- matched instruction:   %11 = load i32, i32* %3, align 4, !dbg !27
...

$ bin/locator ../test/pmem/hello_libpmem.bc hello_libpmem.c:64
Successfully parsed ../test/pmem/hello_libpmem.bc
Matched function <write_hello_string>()@test/pmem/hello_libpmem.c:51,75
- matched instruction:   %13 = load i8*, i8** %5, align 8, !dbg !37
- matched instruction:   %14 = load i8*, i8** %3, align 8, !dbg !38
- matched instruction:   %15 = call i8* @strcpy(i8* %13, i8* %14) #8, !dbg !39
```

## Instrumented program to print address
```
$ cd build
$ ../scripts/instrument-compile.sh --load-store --output loop1-instrumented ../test/loop1.bc
Instrumented call to __arthas_addr_tracker_init in main
Instrumented call to __arthas_addr_tracker_finish in main
Instrumented 76 regular load/store instructions in total

$ ./loop1-instrumented
openning address tracker output file pmem_addr_pid_16062.dat
Enter input: 30
result for input 30 is 362880
```

Note that since `loop1.c` is not a pmem test case, we added the flag `-load-store`
to instrument the regular load/store instructions for testing purpose. For
a pmem test case, you should remove the `-load-store` command line flag.

The tracing result file is `pmem_addr_pid_16062.dat`, where `16062` in the pid 
of a particular run of the instrumented program. A sample content in the 
tracing result file:

```
0x7fffc57f636c,260
0x7fffc57f6368,261
0x7fffc57f6360,262
0x7fffc57f6368,263
0x7fffc57f635c,267
0x7fffc57f62d8,227
...
```

Note that the second column represents a GUID of the instrumented LLVM instruction 
that causes this dynamic address to be printed. Using GUID instead of the
actual instruction makes the tracing efficient. The introduced indirection, 
however, means that we need to look up what instruction this GUID represents.
The mapping is generated during the **static instrumentation** phase, written
into a file that by default is `hook_guid.dat`. This file records detailed
information (<file, function, line number, instruction full string representation>) 
to locate a corresponding instruction. A sample content in the guid mapping file:

```
227##/home/ryan/project/Arthas/test##loop1.c##add##20##  store i32 %0, i32* %3, align 4
...
260##/home/ryan/project/Arthas/test##loop1.c##main##51##  store i32 0, i32* %3, align 4
261##/home/ryan/project/Arthas/test##loop1.c##main##51##  store i32 %0, i32* %4, align 4
262##/home/ryan/project/Arthas/test##loop1.c##main##51##  store i8** %1, i8*** %5, align 8
263##/home/ryan/project/Arthas/test##loop1.c##main##55##  %12 = load i32, i32* %4, align 4, !dbg !25
264##/home/ryan/project/Arthas/test##loop1.c##main##56##  %16 = load i8**, i8*** %5, align 8, !dbg !29
265##/home/ryan/project/Arthas/test##loop1.c##main##56##  %19 = load i8*, i8** %18, align 8, !dbg !29
```

## Slicing a program given a fault instruction
```
$ cd build
$ bin/slicer -criteria hello_libpmem.c:64 -inst '%14 = load i8*, i8** %5, align 8, !dbg !37' -dir=backward ../test/pmem/hello_libpmem.bc
Successfully parsed ../test/pmem/hello_libpmem.bc
Found slice instruction %14 = load i8*, i8** %5, align 8, !dbg !37
Begin instruction slice
...
Got dependence graph for function write_hello_string
Computing slice for fault instruction   %14 = load i8*, i8** %5, align 8, !dbg !37
Building a graph for slice 1
Slice graph 1 is constructed
INFO: Sliced away 107 from 116 nodes
INFO: Slice graph has 11 node(s)
Slice graph is written to slices.log
The list of slices are written to slices.log
Destructing slice graph 1
```

## Slicing given a fault instruction + instrumenting the persistent slice

```
$ cd build
$ bin/slicer -criteria hello_libpmem.c:64 -inst '%14 = load i8*, i8** %5, align 8, !dbg !37' -dir=backward -hook-pmem -output hello_libpmem-instrumented.bc ../test/pmem/hello_libpmem.bc
Successfully parsed ../test/pmem/hello_libpmem.bc
Found slice instruction %14 = load i8*, i8** %5, align 8, !dbg !37
Begin instruction slice
...
Instrumented call to __arthas_addr_tracker_init in main
Instrumented call to __arthas_addr_tracker_finish in main
Got dependence graph for function write_hello_string
Computing slice for fault instruction   %14 = load i8*, i8** %5, align 8, !dbg !37
Building a graph for slice 1
Slice graph 1 is constructed
INFO: Sliced away 107 from 116 nodes
INFO: Slice graph has 11 node(s)
Slice graph is written to slices.log
The list of slices are written to slices.log
Destructing slice graph 1
Slice 1 is persistent or mixed, instrument it
Slice 2 is persistent or mixed, instrument it
Slice 3 is persistent or mixed, instrument it
Slice 4 is persistent or mixed, instrument it
Slice 5 is persistent or mixed, instrument it
Instrumented 1 pmem instructions in total
Instrumented program is written into bitcode file hello_libpmem-instrumented.bc
```

This will produce an instrumented bitcode file `hello_libpmem-instrumented.bc`
for the test program `hello_libpmem`. To turn this instrumented bitcode into 
an executable:

```
$ llc -O0 -disable-fp-elim -filetype=asm -o hello_libpmem-instrumented.s hello_libpmem-instrumented.bc
$ gcc -no-pie -O0 -fno-inline -o hello_libpmem-instrumented hello_libpmem-instrumented.s -L analyzer/runtime -l:libAddrTracker.a -lpmem
```

Note that we linked the assembly with both our tracking lib and the PMDK library.

Run the instrumented pmem test case.

```
$ ./hello_libpmem-instrumented -w /mnt/mem/hello_libpmem-instrumented.pm
openning address tracker output file pmem_addr_pid_25647.dat
...
```

Content of the tracing file and guid map file:

```
0x7eff4d000000,200

200##/home/ryan/project/Arthas/test/pmem##hello_libpmem.c##write_hello_string##58##  %10 = call i8* @pmem_map_file(i8* %9, i64 1024, i32 1, i32 438, i64* %6, i32* %7), !dbg !30
```

# Code Style

### Command-Line
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

### Make target
We defined a make target in the CMakeFiles to run `clang-format` on all source
files when invoked. 

```
make format
```

(first time using it should do a `cd build; cmake ..`)

### IDE
If you are using Clion, the IDE supports `.clang-format` style. Go to `Settings/Preferences | Editor | Code Style`, 
check the box `Enable ClangFormat with clangd server`. 

### Vim
`clang-format` can also be integrated with vim [doc](http://clang.llvm.org/docs/ClangFormat.html#clion-integration).
Put the following in your `.vimrc` will automatically run `clang-format` whenever
you save (write) the `.cpp` or `.h` files. The `Ctrl-K` command will format
selected regions.

```
map <C-K> :pyf /opt/software/llvm/3.9.1/dist/share/clang/clang-format.py<cr>
imap <C-K> <c-o>:pyf /opt/software/llvm/3.9.1/dist/share/clang/clang-format.py<cr>

function! Formatonsave()
  let l:formatdiff = 1
  pyf /opt/software/llvm/3.9.1/dist/share/clang/clang-format.py
endfunction
autocmd BufWritePre *.h,*.cc,*.cpp call Formatonsave()
```
