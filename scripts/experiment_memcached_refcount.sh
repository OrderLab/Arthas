#!/bin/bash

root_dir=$(cd "$(dirname "${BASH_SOURCE-$0}")"/..; pwd)
bc_path=$root_dir/eval-sys/memcached/memcached.bc
experiment_dir=$root_dir/experiment/memcached
guid_map=$experiment_dir/memcached-hook-guids.map
instrumented_memcahed=$root_dir/build/memcached-instrumented

# sanity checks
if [ ! -f $bc_path ]; then
  echo "$bc_path does not exist. Have you run scripts/instrument-compile.sh?"
  exit 1
fi

if [ ! -f $guid_map ]; then
  echo "$guid_map is missing."
  exit 1
fi

if [ ! -x $instrumented_memcahed ]; then
  echo "$instrumented_memcahed does not exist"
  exit 1
fi

echo "sanity checks passed"

# make sure python-memcached module is installed
pip install --user python-memcached
