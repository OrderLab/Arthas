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

## Extract PM variables and their slices

```
opt -load build/analyzer/lib/libLLVMSlicer.so -hello < test/loop1.bc > /dev/null
```

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
- users of the pmem variable:   %22 = load i8*, i8** %5, align 8
- users of the pmem variable:   %24 = call i32 @pmem_msync(i8* %22, i64 %23)
- users of the pmem variable:   %26 = load i8*, i8** %5, align 8
- users of the pmem variable:   %27 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([46 x i8], [46 x i8]* @.str.1, i32 0, i32 0), i8* %26)
- users of the pmem variable:   %10 = icmp eq i8* %9, null
- users of the pmem variable:   br i1 %10, label %11, label %12
* Identified pmdk API calls:   %9 = call i8* @pmem_map_file(i8* %8, i64 1024, i32 1, i32 438, i64* %6, i32* %7)
* Identified pmdk API calls:   call void @pmem_persist(i8* %19, i64 %20)
* Identified pmdk API calls:   %24 = call i32 @pmem_msync(i8* %22, i64 %23)
* Identified pmem variable instruction:   %9 = call i8* @pmem_map_file(i8* %8, i64 1024, i32 1, i32 438, i64* %6, i32* %7)
* Identified pmem variable instruction:   %13 = load i8*, i8** %5, align 8
* Identified pmem variable instruction:   %15 = call i8* @strcpy(i8* %13, i8* %14) #7
* Identified pmem variable instruction:   %19 = load i8*, i8** %5, align 8
* Identified pmem variable instruction:   call void @pmem_persist(i8* %19, i64 %20)
* Identified pmem variable instruction:   %22 = load i8*, i8** %5, align 8
* Identified pmem variable instruction:   %24 = call i32 @pmem_msync(i8* %22, i64 %23)
* Identified pmem variable instruction:   %26 = load i8*, i8** %5, align 8
...
```

Specify a target function to extract the pmem variables from that function.
```
$ opt -load analyzer/lib/libLLVMPMem.so -pmem -target-functions write_vector < ../test/pmem/pmem_vector.bc > /dev/null
```

The output is:
```
[write_vector]
- this instruction creates a pmem variable:   %33 = call i8* @pmemobj_direct_inline(i64 %30, i64 %32)
- users of the pmem variable:   %34 = bitcast i8* %33 to %struct.vector*
- users of the pmem variable:   %35 = load %struct.vector*, %struct.vector** %10, align 8
- users of the pmem variable:   %36 = getelementptr inbounds %struct.vector, %struct.vector* %35, i32 0, i32 0
- users of the pmem variable:   %60 = load i32*, i32** %12, align 8
- users of the pmem variable:   %61 = bitcast i32* %60 to i8*
- users of the pmem variable:   %62 = call i32 @pmemobj_tx_add_range_direct(i8* %61, i64 4)
- users of the pmem variable:   %63 = load i32*, i32** %12, align 8
- users of the pmem variable:   call void @pointer_set(i32* %63, i32 5)
- users of the pmem variable:   %106 = load %struct.vector*, %struct.vector** %10, align 8
- users of the pmem variable:   %107 = getelementptr inbounds %struct.vector, %struct.vector* %106, i32 0, i32 0
- users of the pmem variable:   %109 = load %struct.vector*, %struct.vector** %10, align 8
- users of the pmem variable:   %110 = getelementptr inbounds %struct.vector, %struct.vector* %109, i32 0, i32 1
- users of the pmem variable:   %112 = load %struct.vector*, %struct.vector** %10, align 8
- users of the pmem variable:   %113 = getelementptr inbounds %struct.vector, %struct.vector* %112, i32 0, i32 2
* Identified pmdk API calls:   %33 = call i8* @pmemobj_direct_inline(i64 %30, i64 %32)
* Identified pmem variable instruction:   %33 = call i8* @pmemobj_direct_inline(i64 %30, i64 %32)
* Identified pmem variable instruction:   %34 = bitcast i8* %33 to %struct.vector*
* Identified pmem variable instruction:   %35 = load %struct.vector*, %struct.vector** %10, align 8
* Identified pmem variable instruction:   %36 = getelementptr inbounds %struct.vector, %struct.vector* %35, i32 0, i32 0
* Identified pmem variable instruction:   %60 = load i32*, i32** %12, align 8
* Identified pmem variable instruction:   %61 = bitcast i32* %60 to i8*
* Identified pmem variable instruction:   %62 = call i32 @pmemobj_tx_add_range_direct(i8* %61, i64 4)
* Identified pmem variable instruction:   %63 = load i32*, i32** %12, align 8
* Identified pmem variable instruction:   call void @pointer_set(i32* %63, i32 5)
...
```

TBA

## Dump program dependency graph

```
cd build
bin/llvm-dg-dump ../test/field-sensitive.bc > field-sensitive-dg.dot
bin/llvm-pta-dump ../test/field-sensitive.bc > field-sensitive-pta.dot

# convert the .dot graph files to PDF

dot -Tpdf field-sensitive-dg.dot -o field-sensitive-dg.pdf
dot -Tpdf field-sensitive-pta.dot -o field-sensitive-pta.pdf
```

scp the PDF files to local laptop for viewing the graph. The dependence 
or pointer graph could be very large that you need to zoom into the graph.
On Mac, the Preview app may not be able to handle the zooming well. The Adobe 
PDF Reader (free version) app is more robust to view these pdf files.


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
