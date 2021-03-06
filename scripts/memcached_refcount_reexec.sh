#!/bin/bash

scripts_dir=$(cd "$(dirname "${BASH_SOURCE-$0}")"; pwd)

(sleep 1 ; timeout 8 $scripts_dir/memcached_refcount_twoclients.sh) & X=$!
(sleep 2 ; $scripts_dir/killScript) & Y=$!
($scripts_dir/memcached-refcount -p 11211 -U 11211) & 
echo $! > memcached.pid
wait $X
retn_code=$?
echo "job X returned $?"
wait
echo $X
echo $Y
echo $Z
exit $retn_code
