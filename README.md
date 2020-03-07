# Source code repository for Arthas

# Build

## Compile the tool

```
mkdir build
cd build && cmake ..
make -j4
```

## Compile the test programs

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


## Compile a large system for analysis

For large software, we need to modify its build system to use clang for compilation.
The most systematic way is using the [WLLVM](https://github.com/travitch/whole-program-llvm) 
wrapper.

TBA

# Usage

Arthas consists of the analyzer, detector and reactor components. The usage
instruction for each component is in the README of its corresponding directory.

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
