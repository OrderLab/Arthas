# Arthas

## Step 0: ssh access

TODO

## Step 1: Installation

In order to get any bug running, you’ll have to install these:
1. Install vanilla PMDK
2. Installing Arthas’ checkpoint component (modified pmdk)
3. Install wllvm
4. Install Arthas
5. Install the System (Memcached, Redis, etc.) that you’re testing on (if you want to test on our bugs in the paper, you’ll need to do this for every bug f1-f12)

### Step 1.1: Install vanilla PMDK ###

Install Intel’s PMDK persistent memory library from: https://github.com/pmem/pmdk 
We used version 1.4.2 of pmdk, but later versions should also be fine. (git checkout tags/1.4.2)


### Step 1.2: Install Arthas’ checkpoint component (modified PMDK) ###

Install Arthas's checkpoint component (which is a modified version of pmdk). Install from https://github.com/OrderLab/PMDK-Arthas 
The installation instructions should mostly be the same except, skip the checkout tags/1.4.2 (pmdk arthas’ current version is fine) and you won’t have to install most of the dependencies since they’ll already be taken care of from step 1.1. 

```
export PMDK_HOME=[path to modified PMDK directory]
```
Make sure to set this environment variable either using the command line or adding to the ~/.bashrc and calling source ~/.bashrc to make it permanent.

### Step 1.3 Installing wllvm ###

Install wllvm from https://github.com/travitch/whole-program-llvm 
Follow the instructions on the github page.

Set environment variables for wllvm to use in systems in Step 3.
``` 
export LLVM_COMPILER=clang
export LLVM_HOME=/opt/software/llvm/3.9.1/dist
export LLVM_COMPILER_PATH=$LLVM_HOME/bin
export PATH=$LLVM_COMPILER_PATH:$PATH
```

### 1.4 Installing Arthas ###
Follow the instructions here to install Arthas: https://github.com/OrderLab/arthas 

### 1.5 Installing the System ###
This varies for each system, you can git clone from each of these github repos that represent a different system:  

Memcached: https://github.com/OrderLab/Arthas-eval-Memcached  
Redis: https://github.com/OrderLab/Arthas-eval-Redis   
CCEH: https://github.com/OrderLab/Arthas-Eval-CCEH   
PMEMKV: https://github.com/OrderLab/Arthas-eval-pmemkv   
Pelikan: https://github.com/OrderLab/Arthas-eval-pelikan   

Then do a checkout the git branch that corresponds to the bug you are trying to run (More details on this later, but for example for bug f4, the Memcached Int Overflow bug, git clone Arthas-eval-Memcached and then call ‘git checkout int_overflow’ )

#### 1.5.1 Memcached ####
The github branches that you will need to install for Memcached's bugs include   
Bug f1: Memcached Refcount Bug
```
git checkout refcount
```
Bug f2: Memcached flush_all Bug
```
git checkout flush_all
```
Bug f3: Memcached Hashtable lock data race Bug
```
git checkout race_condition
```
Bug f4: Memcached int overflow Bug
```
git checkout int_overflow
```
Bug f5: Memcached Rehashing Flag Bit Flip Bug
```
git checkout rehashing
```

Then in order to compile our PM Memcached Port you will run these commands
```
cd memcached
./autogen.sh
CC=wllvm CFLAGS="-g -O0" ./configure --enable-pslab
```
At this point you will have to add the line 
```
LDFLAGS = -Wl,--no-as-needed -ldl 
```
to the Makefile. Search for the phrase 'LDFLAGS =' and there should be an empty space next to the equals sign, add in “-Wl,--no-as-needed -ldl” right next to the equals sign.

Then run
```
make -j16
extract-bc memcached
```

#### 1.5.2 Redis ####
The github branches that you will need to install for Redis's bugs include 

Bug f6: Redis Listpack Bug
```
git checkout listpack_redis
```
Bug f7: Redis Refcount Bug
```
git checkout refcount
```
Bug f8: Redis Slowlog Memory Leak Bug
```
git checkout memleak
```

First, add your vanilla pmdk folder to the deps folder in Redis (cp or mv is fine)
```
cp -r [path to vanilla pmdk]/pmdk [path to Redis]/deps
cd src
./mscript
```

For bug f8, we don't need the bytecode so we instead run:
```
cd src
make USE_PMEM=yes -j16
```

#### 1.5.3 CCEH ####
CCEH maintains one bug  
f9: CCEH directory doubling bug
```
git checkout double
```

Then run the commands
```
./fileScript
cd CCEH-PMDK
make -j16
```

#### 1.5.4 Pelikan ####
Pelikan includes   
Bug f10: Value len overflow
```
git checkout val
```
Due to time constraints, we weren't able to bring f11 to the artifact evaluation. 

After checkout the branch then you can run 
```
mkdir build && cd build
CC=wllvm cmake ..
make USE_PMEM=yes -j16
cd _bin
extract-bc pelikan_twemcache
opt -loweratomic <pelikan_twemcache.bc> pelikan_twemcache-lower.bc
```

#### 1.5.5 PMEMKV ####
Pmemkv includes   
Bug f12: PMEMKV asynchronous lazy free bug
```
git checkout memleak
```

Then simply run 
```
make -j16
```

Once you are done installing all 5 systems, make sure to insert the corresponding directories to each of your directories corresponding to each bug in the Arthas.conf file found in Arthas 
```
cd [Arthas directory]
vim arthas.conf
```

## Step 2: Running Arthas on the bugs

After completing the above steps we can now run Arthas on bugs f1-f7 and f9-f10.   
Bugs f8 and f12 are memory leak bugs and will be run in a different manner. 

```
cd [Arthas directory]
cd scripts
./f1
./f2
./f3
```
The results/data will appear in the results folder. You will see the time taken for Arthas to resolve the issues in the .times files 
and see the reverted data in the results/result.txt
