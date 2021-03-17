#!/bin/bash

function kill_pid_file()
{
  local pid_file=$1
  local process_name=$2
  if [ -f $pid_file ]; then
    echo "$process_name is running. Killing it..."
  else
    return
  fi
  local pid_content=$(cat $pid_file)
  if [ -z "$pid_content" ];then
    echo "Empty pid file. Skip"
    rm $pid_file
    return
  fi
  kill -9 $pid_content
  if [ $? -eq 0 ]; then
    echo "Stopped $process_name (PID $pid_content)"
    rm $pid_file
  else
    echo "Failed to stop $process_name (PID $pid_content)"
  fi
}

root_dir=$(cd "$(dirname "${BASH_SOURCE-$0}")"/..; pwd)
scripts_dir=$(cd "$(dirname "${BASH_SOURCE-$0}")"; pwd)
bc_path=$root_dir/eval-sys/memcached/memcached.bc
experiment_dir=$root_dir/experiment/memcached
guid_map=$experiment_dir/memcached-hook-guids.map
vanilla_memcached=$root_dir/eval-sys/memcached/memcached
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

cd $experiment_dir

# clean up previous instance first
kill_pid_file memcached.pid "Memcached server"
rm /mnt/pmem/memcached.pm 2>&1 >/dev/null
rm output_log

echo "Starting instrumented Memcached..."
$instrumented_memcahed -p 11211 -U 11211 & echo $! > memcached.pid

memcached_pid=$(cat memcached.pid)
if [ -n "$memcached_pid" -a -f /proc/$memcached_pid/exe ]; then
  echo "Successfully started Memcached (PID $memcached_pid)"
else
  echo "Failed to start Memcached." 
  exit
fi
sleep 5  # wait until Memcached fully starts

echo "Inserting workload for 2 mins and 30 seconds"
python $scripts_dir/memcached_inserts.py & echo $! > workload_driver.pid
sleep 150

kill_pid_file workload_driver.pid "Memcached client"
echo "Finished Workload insertion for 2 mins and 30 seconds"
python $scripts_dir/memcached_stats.py

#(sleep 2; kill_pid_file memcached.pid "Memcached server") &
(sleep 2; $scripts_dir/memcached_kill.sh $experiment_dir) &
($scripts_dir/memcached_refcount_bug_trigger.sh > refcount_bug_trigger.out 2>&1) &
wait
echo "Triggered Memcached refcount bug"

# make sure we are in experiment dir
cd $experiment_dir

echo "(Optional) Getting rid of duplicate GUID entries"
pmem_addr="pmem_addr_pid_${memcached_pid}.dat"
perl -i -ne 'print if ! $x{$_}++' $pmem_addr
echo "Finished with duplicate removal"

echo "Getting the Arthas reactor server ready"
prepared=output_log

$root_dir/build/bin/reactor_server -g memcached-hook-guids.map -b $root_dir/build/memcached-instrumented.bc -a $pmem_addr -p /mnt/pmem/memcached.pm -t store.db -l libpmemobj -n 1 --rxcmd "$scripts_dir/memcached_refcount_reexec.sh" &
echo $! > reactor_server.pid &
while :
do
  if test -f "$prepared"; then
    break
  fi
  sleep 1
done
echo "Arthas reactor server is ready!!"

# starting the client request
echo "Query Arthas reactor server"
$root_dir/build/bin/reactor_client -i '%72 = load %struct._stritem*, %struct._stritem** %7, align 8, !dbg !3120' -c 'assoc.c:107'
#$root_dir/build/bin/reactor_client -i '%82 = load %struct._stritem*, %struct._stritem** %7, align 8, !dbg !3120' -c 'assoc.c:107'
tput setaf 2; echo "Recovery finished!!"

# clean up
kill_pid_file reactor_server.pid "Arthas reactor server"
rm output_log
