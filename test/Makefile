SRC = $(wildcard *.c)
BITCODES = $(patsubst %.c, %.bc, $(SRC))
SUBDIRS = pmem

.PHONY: all clean subdir

all: $(BITCODES) subdir

subdir:
	for dir in $(SUBDIRS); do \
		make -C $$dir; \
	done

%.bc: %.c
	clang -c -emit-llvm $< -o $@

clean:
	rm *.bc
	for dir in $(SUBDIRS); do \
		make -C $$dir clean; \
	done
