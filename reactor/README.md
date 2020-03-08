# Reactor

The runtime meta environment that reacts on a suspected hard fault and brings
the system back to a normal state.

# Usage

## Rollback with reverter

```
$ cd build
$ bin/reator -h
Usage: bin/reactor [-h] [OPTION]

Options:
  -h, --help                   : show this help
  -p, --pmem-file <file>       : path to the target system's persistent memory file
  -t, --pmem-layout <layout>   : the PM file's layout name
  -l, --pmem-lib <library>     : the PMDK library: libpmem, libpmemobj
  -n, --ver <number>           : the version number to revert for the 1st
                                 coarse-grained reversion attempt
  -g, --guid-map <file>        : path to the static GUID map file
  -a, --addresses <file>       : path to the dynamic address trace file

```

This will enact coarse-grained reversion on the passed pmem file and then try and rollback changes
one version at a time. After each rollback, it will try to re execute the system using the passed in script of inputs.
If it fails then it will retry until it successfully runs or until you reach a Max number of reversion times.

