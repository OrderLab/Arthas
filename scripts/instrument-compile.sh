#!/bin/bash

# The Arthas Project
#
# Copyright (c) 2019, Johns Hopkins University - Order Lab.
#
#    All rights reserved.
#    Licensed under the Apache License, Version 2.0 (the "License");

function display_usage()
{
  cat <<EOF
Usage:
  $0 [options] SOURCE_BC_FILE

  -h, --help:       display this message

      --plugin:     path to libInstrument.so, default is build/analyzer/lib

      --load-store: instrument regular load/store instructions

  -l, --link:       additional link flags to pass to GCC

  -r, --runtime:    path to libAddrTracker.a/.so runtime 

      --dry-run:    dry run, do not

  -o, --output:     output file name
EOF
}

function parse_args()
{
  args=()
  while [[ "$#" -gt 0 ]]
  do
    case $1 in
      -o|--output)
        output="$2"
        shift 2
        ;;
      --plugin)
        plugin_path="$2"
        shift 2
        ;;
      --load-store)
        load_store=1
        shift 1
        ;;
      -l|--link)
        link_flags="$2"
        shift 2
        ;;
      --dry-run)
        maybe=echo
        shift
        ;;
      -r|--runtime)
        runtime_path="$2"
        shift 2
        ;;
      -h|--help)
        display_usage
        exit 0
        ;;
      *)
        args+=("$1")
        shift
        ;;
    esac
  done
}

this_dir=$(cd "$(dirname "${BASH_SOURCE-$0}")"; pwd)
build_dir=$(cd "${this_dir}/../build"; pwd)
output=
plugin_path=
maybe=
runtime_path=
link_libs=
load_store=0
plugin_args=""
link_flags=""

parse_args "$@"
set -- "${args[@]}"
if [ $# -ne 1 ]; then
  display_usage
  exit 1
fi
source_bc_file=$1
if [[ $source_bc_file != *.bc ]]; then
  echo "Source file must be in .bc format"
  exit 1
fi
if [ ! -f $source_bc_file ]; then
  echo "Input bitcode file $source_bc_file does not exist"
  exit 1
fi
if [ -z "$plugin_path" ]; then
  plugin_path=${build_dir}/analyzer/lib/libInstrument.so
fi
if [ ! -f $plugin_path ]; then
  echo "Plugin $plugin_path not found"
  exit 1
fi
if [ -z "$runtime_path" ]; then
  runtime_path=${build_dir}/analyzer/runtime
fi
if [ ! -d "$runtime_path" ]; then
  echo "Runtime path $runtime_path does not exist"
  exit 1
fi
runtime_lib="$runtime_path"/libAddrTracker.a
if [ ! -f  $runtime_lib ]; then
  echo "Runtime library $runtime_lib not found"
  exit 1
fi 
if [ -z "$output" ]; then
  output_base="${source_bc_file%.*}"-instrumented
  output_bc=${output_base}.bc
  output_asm=${output_base}.s
  output_exe=${output_base}
else
  output_bc=${output}.bc
  output_asm=${output}.s
  output_exe=${output}
fi

if [ $load_store -ne 0 ]; then
  plugin_args="$plugin_args -regular-load-store"
fi

if [ -z "$link_flags" ] && [ $load_store -eq 0 ]; then
  # if link flags is not supplied and it's not a regular load-store 
  # instrumentation, try to be smart here by automatically add the 
  # -lpmem link flag to link with libpmem. If the program is linked
  # with libpmemobj instead, should pass the link flag explicitly.
  link_flags="-lpmem -lpmemobj"
fi

$maybe opt -load $plugin_path -instr $plugin_args $source_bc_file -o $output_bc
$maybe llc -O0 -disable-fp-elim -filetype=asm -o $output_asm $output_bc
$maybe llvm-dis $output_bc
# linking with shared runtime lib, flexible but slower
# $maybe gcc -no-pie -O0 -fno-inline -o $output_exe $output_asm -L $runtime_path -lAddrTracker
# linking with static runtime lib, less flexible but faster
$maybe gcc -no-pie -O0 -fno-inline -o $output_exe $output_asm -L $runtime_path -l:libAddrTracker.a $link_flags
