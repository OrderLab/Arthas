# Reactor

The runtime meta environment that reacts on a suspected hard fault and brings
the system back to a normal state.

# Usage

## Rollback with reverter

```
./reversion instrumentation_out <pmem_file of crashed system> <pmem layout name> <version # to revert to for 1st coarse attempt> <rerun system commands> <pmem library you are using>
```

This will enact coarse-grained reversion on the passed pmem file and then try and rollback changes
one version at a time. After each rollback, it will try to re execute the system using the passed in script of inputs.
If it fails then it will retry until it successfully runs or until you reach a Max number of reversion times.

