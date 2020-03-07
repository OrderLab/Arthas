# Arthas static analyzers

The static analyzer component of Arthas based on LLVM. It consists of a 
number of tools that analyze the persistent variables, their dependency graph, 
and slices to be used for reactor. The analyzer is also responsible for
inserting instrumentation points in a target system for obtaining runtime
information such as the dynamic address of the persistent variable in 
a slice.

# Usage

## Extract persistent memory variables
Run our LLVMPMem pass on a persistent memory test program:

```
$ cd build
$ bin/extractor ../test/pmem/hello_libpmem.bc
```

The output is:

```
* Identified pmdk API calls:   %10 = call i8* @pmem_map_file(i8* %9, i64 1024, i32 1, i32 438, i64* %6, i32* %7), !dbg !30
* Identified pmdk API calls:   call void @pmem_persist(i8* %22, i64 %23), !dbg !47
* Identified pmdk API calls:   %27 = call i32 @pmem_msync(i8* %25, i64 %26), !dbg !50
* Identified pmem variable instruction:   %10 = call i8* @pmem_map_file(i8* %9, i64 1024, i32 1, i32 438, i64* %6, i32* %7), !dbg !30
* Identified pmem variable instruction:   %14 = load i8*, i8** %5, align 8, !dbg !37
* Identified pmem variable instruction:   %16 = call i8* @strcpy(i8* %14, i8* %15) #8, !dbg !39
...
* Identified pmem variable instruction:   br i1 %11, label %12, label %13, !dbg !33
* Identified pmem region [  %10 = call i8* @pmem_map_file(i8* %9, i64 1024, i32 1, i32 438, i64* %6, i32* %7), !dbg !30,i64 1024]
* Identified pmdk API calls:   %7 = call i8* @pmem_map_file(i8* %6, i64 1024, i32 1, i32 438, i64* %4, i32* %5), !dbg !28
* Identified pmem variable instruction:   %7 = call i8* @pmem_map_file(i8* %6, i64 1024, i32 1, i32 438, i64* %4, i32* %5), !dbg !28
...
```

You can also specify a target function to extract the pmem variables within 
that function.

```
$ cd build 
$ bin/extractor -function write_vector ../test/pmem/pmem_vector.bc
* Identified pmdk API calls:   %33 = call i8* @pmemobj_direct_inline(i64 %30, i64 %32), !dbg !69
* Identified pmem variable instruction:   %33 = call i8* @pmemobj_direct_inline(i64 %30, i64 %32), !dbg !69
* Identified pmem variable instruction:   %34 = bitcast i8* %33 to %struct.vector*, !dbg !69
* Identified pmem variable instruction:   %35 = load %struct.vector*, %struct.vector** %10, align 8, !dbg !74
* Identified pmem variable instruction:   %36 = getelementptr inbounds %struct.vector, %struct.vector* %35, i32 0, i32 0, !dbg !75
...
```

Note that `bin/extractor` is a standalone executable tool. It internally 
invokes the LLVMPMem pass we wrote. You can also invoke this pass 
with the LLVM `opt` tool, which is slightly more cumbersome to type:

```
$ cd build
$ opt -load analyzer/lib/libLLVMPMem.so -pmem < test/pmem/hello_libpmem.bc > /dev/null
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

## Instrumenting program to print address

### Instrumenting regular memory load/store

```
$ cd build
$ ../scripts/instrument-compile.sh --load-store --output loop1-instrumented ../test/loop1.bc
Instrumented call to __arthas_addr_tracker_init in main
Instrumented call to __arthas_addr_tracker_finish in main
Hook map to loop1-hook-guids.map
Instrumented 76 regular load/store instructions in total
The hook GUID map is written to loop1-hook-guids.map
```

Note that since `loop1.c` is *not* a pmem test case, we added the flag **`-load-store`**
to instrument the regular load/store instructions for testing purpose. For
a pmem test case, you should remove the `-load-store` command line flag.

After this step, an instrumented executable `<input_file>-instrumented` is 
produced. In addition, a static mapping table is generated to record the 
hook GUID and the information (<file, function, line number, instruction full 
string representation>) to locate a corresponding instruction. This mapping table is written
into a file that by default is `<input_file>-hook-guid.dat`.  

```
$ cat loop1-hook-guids.map
227##/home/ryan/project/Arthas/test##loop1.c##add##20##  store i32 %0, i32* %3, align 4
...
260##/home/ryan/project/Arthas/test##loop1.c##main##51##  store i32 0, i32* %3, align 4
261##/home/ryan/project/Arthas/test##loop1.c##main##51##  store i32 %0, i32* %4, align 4
262##/home/ryan/project/Arthas/test##loop1.c##main##51##  store i8** %1, i8*** %5, align 8
263##/home/ryan/project/Arthas/test##loop1.c##main##55##  %12 = load i32, i32* %4, align 4, !dbg !25
264##/home/ryan/project/Arthas/test##loop1.c##main##56##  %16 = load i8**, i8*** %5, align 8, !dbg !29
265##/home/ryan/project/Arthas/test##loop1.c##main##56##  %19 = load i8*, i8** %18, align 8, !dbg !29
```

Now run the instrumented executable `loop1-instrumented`:

```
$ ./loop1-instrumented
openning address tracker output file pmem_addr_pid_16062.dat
Enter input: 30
result for input 30 is 362880
```

The tracing result file is `pmem_addr_pid_16062.dat`, where `16062` in the pid 
of a particular run of the instrumented program: 

```
$ cat pmem_addr_pid_16062.dat
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
however, means that we need to look up what instruction this GUID represents
in the static GUID mapping file that was generated in the static instrumentation
phase.

### Instrumenting persistent memory accesses

For instrumenting a persistent memory program, we should *not* use the `-load-store` 
command line flag. In addition, we need to pass the correct link flag such
as linking with `libpmem` or `libpmemobj` to GCC to produce the instrumented
executable. Without the flag, we'll encounter `undefined reference to XXX`
issue. The link flag can be passed to the instrumentation script with 
`--link "<flags"`.

```
$ cd build
$ ../scripts/instrument-compile.sh --link "-lpmem" --output hello_libpmem-instrumented ../test/pmem/hello_libpmem.bc

Instrumented call to __arthas_addr_tracker_init in main
Instrumented call to __arthas_addr_tracker_finish in main
Instrumented 9 pmem instructions in total
The hook GUID map is written to hello_libpmem-hook-guids.map
```

For convenience, we'll automatically add "-lpmem" flag if the `-load-store` 
is not specified and `--link` option is empty. Therefore, you can omit the 
`--link` option above:

```
$ ../scripts/instrument-compile.sh --output hello_libpmem-instrumented ../test/pmem/hello_libpmem.bc
```

If the to-be-instrumented program needs to be linked with another library, e.g.,
the `libpmemobj`, you still need to specify the link flag:

```
$ ../scripts/instrument-compile.sh --link "-lpmemobj" --output hello_libpmemobj-instrumented ../test/pmem/hello_libpmemobj.bc
```

After instrumentation, when you run the instrumented program, similar as the
regular program, a tracing file `pmem_addr_pid_XXXX.dat` will be generated:

```
$ ./hello_libpmem-instrumented -w /mnt/mem/hello_libpmem.pm
openning address tracker output file pmem_addr_pid_9682.dat
Write the (Hello Persistent Memory!!!) string to persistent memory.
Write the (Second String NEW VALUE HERE!!!) string to persistent memory.

$ cat pmem_addr_pid_9682.dat
0x7f89e9000000,200
0x7ffd495ed498,201
0x7ffd495ed498,202
0x7ffd495ed498,203
0x7ffd495ed498,205
0x7ffd495ed498,206

$ cat hello_libpmem-hook-guids.map
200##/home/ryan/project/Arthas/test/pmem##hello_libpmem.c##write_hello_string##58##  %10 = call i8* @pmem_map_file(i8* %9, i64 1024, i32 1, i32 438, i64* %6, i32* %7), !dbg !30
201##/home/ryan/project/Arthas/test/pmem##hello_libpmem.c##write_hello_string##64##  %14 = load i8*, i8** %5, align 8, !dbg !37
202##/home/ryan/project/Arthas/test/pmem##hello_libpmem.c##write_hello_string##65##  %18 = load i8*, i8** %5, align 8, !dbg !40
203##/home/ryan/project/Arthas/test/pmem##hello_libpmem.c##write_hello_string##68##  %24 = load i8*, i8** %5, align 8, !dbg !45
204##/home/ryan/project/Arthas/test/pmem##hello_libpmem.c##write_hello_string##70##  %28 = load i8*, i8** %5, align 8, !dbg !48
205##/home/ryan/project/Arthas/test/pmem##hello_libpmem.c##write_hello_string##73##  %34 = load i8*, i8** %5, align 8, !dbg !56
206##/home/ryan/project/Arthas/test/pmem##hello_libpmem.c##write_hello_string##75##  %41 = load i8*, i8** %5, align 8, !dbg !63
207##/home/ryan/project/Arthas/test/pmem##hello_libpmem.c##read_hello_string##90##  %7 = call i8* @pmem_map_file(i8* %6, i64 1024, i32 1, i32 438, i64* %4, i32* %5), !dbg !28
208##/home/ryan/project/Arthas/test/pmem##hello_libpmem.c##read_hello_string##96##  %11 = load i8*, i8** %3, align 8, !dbg !35
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
