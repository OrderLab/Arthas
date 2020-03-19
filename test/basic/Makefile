CC = gcc

SRCS = $(wildcard *.c)
EXES = $(patsubst %.c, %, $(SRCS))
BITCODES = $(patsubst %.c, %.bc, $(SRCS))
ASSEMBLYS = $(patsubst %.bc, %.ll, $(BITCODES))

.PHONY: all clean

all: $(EXES) $(BITCODES) $(ASSEMBLYS)

%: %.c
	$(CC) $< -o $@

%.bc: %.c
	clang -c -g -O0 -emit-llvm $< -o $@

%.ll: %.bc
	llvm-dis $<

clean: 
	rm -f *.o $(EXES) $(BITCODES) $(ASSEMBLYS)
