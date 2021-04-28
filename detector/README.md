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


## Soft and Hard Fault Analyze ##
Run the detector's fault analyzer to see if the fault is a hard fault or soft fault:

```
./Detector fault
```

make sure that the two runs of your system are logged to two different gdb logs called 'gdb.txt' and 'gdb2.txt'

[//]: <> (set logging on: set pagination off, thread apply all bt full, r)

