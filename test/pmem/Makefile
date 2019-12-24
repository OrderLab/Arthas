LIBPMEM_CFLAGS = $(shell pkg-config --cflags libpmem) -g -O0
LIBPMEM_LDFLAGS = $(shell pkg-config --libs libpmem) -g -O0

LIBPMEMOBJ_CFLAGS = $(shell pkg-config --cflags libpmemobj) -g -O0
LIBPMEMOBJ_LDFLAGS = $(shell pkg-config --libs libpmemobj) -g -O0

CC = gcc
RM = rm

EXES = hello_libpmem hello_libpmemobj
SRC = $(wildcard *.c)
BITCODES = $(patsubst %.c, %.bc, $(SRC))
SUBDIRS = mapreduce pmem_leak simple_grep

.PHONY: all clean subdir

all: $(EXES) $(BITCODES) subdir

subdir:
	for dir in $(SUBDIRS); do \
		make -C $$dir; \
	done

hello_libpmem.o: hello_libpmem.c
	$(CC) -c $< -o $@ $(LIBPMEM_CFLAGS)

hello_libpmemobj.o: hello_libpmemobj.c
	$(CC) -c $< -o $@ $(LIBPMEMOBJ_CFLAGS)

hello_libpmem: hello_libpmem.o 
	 $(CC) -o $@ $< $(LIBPMEM_LDFLAGS)

hello_libpmemobj: hello_libpmemobj.o
	 $(CC) -o $@ $< $(LIBPMEMOBJ_LDFLAGS)

%.bc: %.c
	clang -c -emit-llvm $< -o $@

clean: 
	rm -f *.o $(EXES) $(BITCODES)
	for dir in $(SUBDIRS); do \
		make -C $$dir clean; \
	done
