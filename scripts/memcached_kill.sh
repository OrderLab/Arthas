#!/bin/bash

if [ $# -ne 0 ]; then
  cd $1
fi

if [ ! -f memcached.pid ]; then
  echo "No prior run found. Skip"
  exit 0
fi
memcached_pid=$(cat memcached.pid)
if [ -z "$memcached_pid" ];then
  echo "Empty pid file. Skip"
  rm memcached.pid
  exit 0
fi
kill -9 $memcached_pid
if [ $? -eq 0 ]; then
  echo "Stopped memcached server (PID $memcached_pid)"
  rm memcached.pid
else
  echo "Failed to stop memcached server (PID $memcached_pid)"
fi
