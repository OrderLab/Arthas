#!/bin/bash

root_dir=$(cd "$(dirname "${BASH_SOURCE-$0}")"/..; pwd)
$root_dir/build/memcached-instrumented -p 11211 -U 11211 & echo $! > memcached.pid
