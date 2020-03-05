# Reactor

The runtime meta environment that reacts on a suspected hard fault and brings
the system back to a normal state.

#Use for reversion

```
./c_reversion instrumentation_out <pmem_file of crashed system> <pmem layout name> <version # to revert to>
```

This will enact coarse-grained reversion on the passed pmem file

