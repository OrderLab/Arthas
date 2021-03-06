#!/bin/bash

scripts_dir=$(cd "$(dirname "${BASH_SOURCE-$0}")"; pwd)
perl $scripts_dir/memcached_refcount_bug_trigger.pl 127.0.0.1:11211
exit $?
