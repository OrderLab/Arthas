FLAGS = -lmemkind

CC = gcc
RM = rm

EXES = memkind_malloc
SRC = $(wildcard *.c)
BITCODES = $(patsubst %.c, %.bc, $(SRC))
ASSEMBLYS = $(patsubst %.bc, %.ll, $(BITCODES))

all: $(EXES) $(BITCODES) $(ASSEMBLYS)

memkind_malloc.o: memkind_malloc.c
	$(CC)  memkind_malloc.c -o memkind_malloc -lmemkind

%.bc: %.c
	clang -c -g -O0 -emit-llvm $< -o $@

%.ll: %.bc
	llvm-dis $<

clean:
	rm -f *.o $(EXES)
