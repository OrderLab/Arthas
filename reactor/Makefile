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

EXES = c_reversion

CC = gcc
RM = rm

all: $(EXES)

c_reversion.o: c_reversion.c
	$(CC) -c $< -o $@ $(LIBPMEMOBJ_CFLAGS)

c_reversion: c_reversion.o
	 $(CC) -o $@ $< $(LIBPMEMOBJ_LDFLAGS)
