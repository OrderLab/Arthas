FLAGS = -lmemkind

CC = gcc
RM = rm

EXES = memkind_malloc
SRC = $(wildcard *.c)
BITCODES = $(patsubst %.c, %.bc, $(SRC))
OBJS = $(patsubst %.c, %.o, $(SRC))
ASSEMBLYS = $(patsubst %.bc, %.ll, $(BITCODES))

all: $(EXES) $(BITCODES) $(ASSEMBLYS)

$(EXES): $(OBJS)
	$(CC) $< -o $@ -lmemkind

%.o: %.c
	$(CC) -c $< -o $@ 

%.bc: %.c
	clang -c -g -O0 -emit-llvm $< -o $@

%.ll: %.bc
	llvm-dis $<

clean:
	rm -f *.o $(EXES)
