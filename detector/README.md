# Detector

Component that detects a potential hard fault in a PM system and pinpoints the 
starting point of the fault

# Usage

## Run ##
Run the system/application you wish to attach the detector to:
```
./Detector run
[executable command you wish to run]
(gdb) set logging on
(gdb) r
(gdb) bt
(gdb) q
```

## Analyze ##
Run the detector's analyzer to get your candidate function and line number:

```
./Detector analyze
```
The resulting candidate function and line number can then be used to pass into the
Extractor which will give you the candidate set of instructions to use in the reactor


[//]: <> (set logging on: set pagination off, thread apply all bt full, r)

