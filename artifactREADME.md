# Arthas 

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

**Note for artifact evaluator:** all dependencies have been installed on the test machine, so no extra setup is needed.

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

## A. Testing Arthas (minimal interaction)

```
$ git clone git@github.com:OrderLab/Arthas.git
$ scripts/artifact_test.sh
```

The test script will build Arthas, custom PMDK, vanilla PMDK, target system Memcached, and finally 
run the Arthas analyzer on the Memcached to instrument it. 

If successful, you should see a Memcached bitcode file in `eval-sys/memcached/memcached.bc` and a Arthas hooks
metadata file in `experiment/memcached/memcached-hook-guids.map`.

You can further test Arthas by a real bug in Memcached:

```
$ scripts/experiment_memcached_refcount.sh
```

This demo script will do the following things: 

1. Start a buggy version of Memcached server (instrumented).
2. Insert some workload to Memcached.
3. Invoke another script to trigger the bug (refcount overflow) and cause Memcached to fail.
4. Start Arthas reactor server.
5. Run the Arthas reactor client to mitigate the failure.

Note that in practice, Arthas reactor server (step 4) is typically started along 
with the target system (step 1). 

If successful, you should see `Recovery finished!!`.

## B. Individual Workflow Walkthrough

### B.1: Building Arthas

```
$ git clone git@github.com:OrderLab/Arthas.git
$ git submodule update --init
$ mkdir build
$ cd build && cmake ..
$ make -j4
```

### B.2: Building Custom PMDK

```
$ cd ../pmdk
$ make -j $(nproc)
```

### B.3: Building Vanilla PMDK

```
$ cd ../vanilla-pmdk
$ make -j $(nproc)
```

### B.4: Building Target System (Memcached)

Make sure the LLVM environment variables are set:
```
export LLVM_COMPILER=clang
export LLVM_HOME=/opt/software/llvm/3.9.1/dist
export LLVM_COMPILER_PATH=$LLVM_HOME/bin
export PATH=$LLVM_COMPILER_PATH:$PATH
export PMDK_HOME=$(pwd)/pmdk
```

Then clone the persistent Memcached code, checkout the buggy branch, and build it:

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

### B.5: Instrumenting Target System

Once we have the `memcached.bc` file, we can then use Arthas's instrumenter to
instrument the Memcached executable with the pmem address trace behavior and
checkpoint component. 

```
$ cd <repo_root>
$ bc_path=eval-sys/memcached/memcached.bc
$ link_flags="-Wl,-rpath=${PMDK_HOME} -lpmemobj -lpthread -levent -levent_core"
$ scripts/instrument-compile.sh -l "$link_flags" --output build/memcached-instrumented $bc_path
```

This will create a new executable in build called memcached-instrumented that we will use to run memcached. 

### B.6: Running Memcached, Inserting a Workload, and Triggering the Bug 

We run the `memcached-instrumented` executable and call a Python script to insert a workload 
of millions of keys to Memcached. We then sleep for 150 seconds (2 minutes 30 
seconds) before killing the python script. We then call a perl script which triggers 
the Memcached refcount bug.

```
$ cd experiment/memcached
$ ../../build/memcached-instrumented -p 11211 -U 11211 & echo $! > memcached.pid
$ memcached_pid=$(cat memcached.pid)

# $memcached_pid should not be empty
$ echo $memcached_pid
$ sleep 5
$ python ../../scripts/memcached_inserts.py & echo $! > workload_driver.pid
$ sleep 150
$ (sleep 2; ../../scripts/memcached_kill.sh ./) &
$ (../../scripts/memcached_refcount_bug_trigger.sh > refcount_bug_trigger.out 2>&1) &
$ wait
$ echo "Triggered Memcached refcount bug"
```


### B.7: Duplicate removal (optional)

This is an optional step where we remove any duplicates in the address trace
file, Arthas will still work even if we don't do this step.

```
$ pmem_addr="pmem_addr_pid_${memcached_pid}.dat"
$ perl -i -ne 'print if ! $x{$_}++' $pmem_addr
```

### B.8: Arthas's Reactor Server

We then run Arthas's reactor server on memcached-instrumented's byte code and
our previous run's mem address trace file, building up the server until it's
ready to serve reversion requests. 

```
$ reexec_path=$(cd ../.. && pwd)/scripts/memcached_refcount_reexec.sh
$ ../../build/bin/reactor_server -g memcached-hook-guids.map -b ../../build/memcached-instrumented.bc -a $pmem_addr -p /mnt/pmem/memcached.pm -t store.db -l libpmemobj -n 1 --rxcmd "$reexec_path" &
```

### B.9: Arthas's Reactor Client

We then use Arthas's reactor client to send a reversion request to Arthas's server

```
$ ../../build/bin/reactor_client -i '%72 = load %struct._stritem*, %struct._stritem** %7, align 8, !dbg !3120' -c 'assoc.c:107'
```

Then Arthas's reactor will begin the reversion and re-execution of Memcached in order to mitigate the fault.
