# if the PMDK_HOME is set, we will use the custom PMDK library to compile 
# and build the test programs: PMDK_HOME=/path/to/custom/pmdk make
# Otherwise, we will use system-wide PMDK library.
ifndef PMDK_HOME
LIBPMEM_CFLAGS = $(shell pkg-config --cflags libpmem) -g -O0
LIBPMEM_LDFLAGS = $(shell pkg-config --libs libpmem) -g -O0
LIBPMEMOBJ_CFLAGS = $(shell pkg-config --cflags libpmemobj) -g -O0
LIBPMEMOBJ_LDFLAGS = $(shell pkg-config --libs libpmemobj) -g -O0
else
LIBPMEM_CFLAGS = -DUSE_PMEM -I $(PMDK_HOME)/src/include -g -O0
LIBPMEM_LDFLAGS = $(PMDK_HOME)/src/nondebug/libpmemobj.a $(PMDK_HOME)/src/nondebug/libpmem.a -g -O0 -ldl -pthread
LIBPMEMOBJ_CFLAGS = -DUSE_PMEM -I $(PMDK_HOME)/src/include -g -O0
LIBPMEMOBJ_LDFLAGS = $(PMDK_HOME)/src/nondebug/libpmemobj.a $(PMDK_HOME)/src/nondebug/libpmem.a -g -O0 -ldl -pthread
endif

CC = gcc
RM = rm

EXES = hello_libpmem hello_libpmemobj pmem_variables pmem_vector
SRC = $(wildcard *.c)
BITCODES = $(patsubst %.c, %.bc, $(SRC))
ASSEMBLYS = $(patsubst %.bc, %.ll, $(BITCODES))
SUBDIRS = mapreduce pmem_leak simple_grep

.PHONY: all clean subdir

all: $(EXES) $(BITCODES) $(ASSEMBLYS) subdir

subdir:
	for dir in $(SUBDIRS); do \
		make -C $$dir; \
	done

hello_libpmem.o: hello_libpmem.c
	$(CC) -c $< -o $@ $(LIBPMEM_CFLAGS)

hello_libpmemobj.o: hello_libpmemobj.c
	$(CC) -c $< -o $@ $(LIBPMEMOBJ_CFLAGS)

pmem_variables.o: pmem_variables.c
	$(CC) -c $< -o $@ $(LIBPMEMOBJ_CFLAGS)

hello_libpmem: hello_libpmem.o 
	 $(CC) -o $@ $< $(LIBPMEM_LDFLAGS)

hello_libpmemobj: hello_libpmemobj.o
	 $(CC) -o $@ $< $(LIBPMEMOBJ_LDFLAGS)

pmem_variables: pmem_variables.o
	 $(CC) -o $@ $< $(LIBPMEMOBJ_LDFLAGS)

pmem_vector: pmem_vector.o
	 $(CC) -o $@ $< $(LIBPMEMOBJ_LDFLAGS)

%.bc: %.c
	clang -c -g -O0 -emit-llvm $< -o $@

%.ll: %.bc
	llvm-dis $<

clean: 
	rm -f *.o $(EXES) $(BITCODES)
	for dir in $(SUBDIRS); do \
		make -C $$dir clean; \
	done
