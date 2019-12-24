SRC = $(wildcard *.c)
BITCODES = $(patsubst %.c, %.bc, $(SRC))

.PHONY: all clean

all: $(BITCODES)

%.bc: %.c
	clang -c -emit-llvm $< -o $@

clean:
	rm *.bc
