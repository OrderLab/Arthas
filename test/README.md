# Testing

## Compiling test cases

Build the test cases with a simple `cd test && make`.

## Slicer

1. items.c
```
$ cd build
$ bin/slicer ../test/memcached/items.bc -criteria items.c:43 -inst '%10 = call i8* %9(%struct.settings* @settings), !dbg !144'

Got dependence graph for function lru_maintainer_thread
Computing slice for fault instruction   store i8* %10, i8** %4, align 8, !dbg !54
Building a graph for slice 1
Slice graph 1 is constructed
INFO: Sliced away 106 from 114 nodes
INFO: Slice graph has 8 node(s)
Slice graph is written to slices.log
The list of slices are written to slices.log
Destructing slice graph 1

$ cat slices.log
...
Slice 3 (n/a):
~lru_maintainer_thread()=>  store i8* %10, i8** %4, align 8, !dbg !54
~lru_maintainer_thread()=>  %10 = call i8* %9(%struct.settings* @settings), !dbg !55
~lru_maintainer_thread()=>  %9 = load i8* (%struct.settings*)*, i8* (%struct.settings*)** %8, align 8, !dbg !56
~lru_maintainer_thread()=>  %8 = getelementptr inbounds %struct.slab_automove_reg_t, %struct.slab_automove_reg_t* %7, i32 0, i32 0, !dbg !56
~lru_maintainer_thread()=>  %7 = load %struct.slab_automove_reg_t*, %struct.slab_automove_reg_t** %3, align 8, !dbg !55
~lru_maintainer_thread()=>  store %struct.slab_automove_reg_t* @slab_automove_default, %struct.slab_automove_reg_t** %3, align 8, !dbg !52
```

2. items.c
```
$ cd build
$ bin/slicer ../test/memcached/items.bc -criteria items.c:52 -inst '%7 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([39 x i8], [39 x i8]* @.str, i32 0, i32 0), i32 %6), !dbg !49'

Got dependence graph for function slab_automove_init
Computing slice for fault instruction   %7 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([39 x i8], [39 x i8]* @.str, i32 0, i32 0), i32 %6), !dbg !49
Building a graph for slice 1
Slice graph 1 is constructed
INFO: Sliced away 107 from 114 nodes
INFO: Slice graph has 7 node(s)
Slice graph is written to slices.log
The list of slices are written to slices.log
Destructing slice graph 1

$ cat slices.log
...
Slice 3 (n/a):
~slab_automove_init()=>  %7 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([39 x i8], [39 x i8]* @.str, i32 0, i32 0), i32 %6), !dbg !49
~slab_automove_init()=>  %6 = load i32, i32* %5, align 8, !dbg !48
~slab_automove_init()=>  %5 = getelementptr inbounds %struct.settings, %struct.settings* %4, i32 0, i32 1, !dbg !48
~slab_automove_init()=>  %4 = load %struct.settings*, %struct.settings** %2, align 8, !dbg !47
~lru_maintainer_thread()=>  store i32 10, i32* getelementptr inbounds (%struct.settings, %struct.settings* @settings, i32 0, i32 1), align 8, !dbg !49
```

3. memcached

```
$ cd build
$ time bin/slicer ../../Arthas-eval/memcached/build/memcached.bc --criteria memcached.c:7354 --inst 'call void @conn_close(%struct.conn* %25), !dbg !8064' 2>&1 | tee memcached_slice.log
...
Trying to create one subgraph for slab_automove_extstore_free() that is connected to main()
Trying to create one subgraph for slab_automove_extstore_run() that is connected to main()
WARNING: RD: Inline assembler found
[RD] error: could not determine the called function in a call via pointer:
do_cache_free::   call void %22(i8* %23, i8* null), !dbg !4220
[RD] error: could not determine the called function in a call via pointer:
do_cache_free::   call void %91(i8* %92, i8* null), !dbg !4285
[slicer] CPU time of pointer analysis: 4.513418e+03 s
[slicer] CPU time of reaching definitions analysis: 2.755668e+02 s
[slicer] CPU time of control dependence analysis: 0.000000e+00 s
Got dependence graph for function event_handler
Computing slice for fault instruction   call void @conn_close(%struct.conn* %25), !dbg !4195
Building a graph for slice 1
Slice graph 1 is constructed
INFO: Sliced away 52770 from 52773 nodes
INFO: Slice graph has 3 node(s)
Slice graph is written to slices.log
The list of slices are written to slices.log
Destructing slice graph 1
```

It took 75 minutes to finish the slicing. The majority of the time is spent
doing pointer analysis. If we disable pointer analysis by modifying line 
106 of `slicer/main.cpp` to 

```cpp
  auto slicer = make_unique<DgSlicer>(M, sliceDir);
  uint32_t flags =
      SlicerDgFlags::INTER_PROCEDURAL | SlicerDgFlags::SUPPORT_THREADS;
```

Then re-run the test

```
$ cd build
$ time bin/slicer ../../Arthas-eval/memcached/build/memcached.bc --criteria memcached.c:7354 --inst 'call void @conn_close(%struct.conn* %25), !dbg !8064' 2>&1 | tee memcached_slice.log
...
[slicer] CPU time of pointer analysis: 0.000000e+00 s
[slicer] CPU time of reaching definitions analysis: 4.489354e+02 s
[slicer] CPU time of control dependence analysis: 0.000000e+00 s
Got dependence graph for function event_handler
Computing slice for fault instruction   call void @conn_close(%struct.conn* %25), !dbg !4195
Building a graph for slice 1
Slice graph 1 is constructed
INFO: Sliced away 52774 from 52777 nodes
INFO: Slice graph has 3 node(s)
Slice graph is written to slices.log
The list of slices are written to slices.log
Destructing slice graph 1

$ cat slices.log
...
Slice 1 (n/a):
~event_handler()=>  call void @conn_close(%struct.conn* %25), !dbg !4195
~event_handler()=>  %25 = load %struct.conn*, %struct.conn** %7, align 8, !dbg !4194
~event_handler()=>  %7 = alloca %struct.conn*, align 8
```
The entire process will take around 3 minutes now. For this case, the slicing 
result is fine without pointer analysis. But note that in general we **should**
enable PTA even if it is time-consuming. Without PTA, the analysis on real
system like memcached will be very inaccurate.
