SRC = $(wildcard *.c)
BITCODES = $(patsubst %.c, %.bc, $(SRC))
ASSEMBLYS = $(patsubst %.bc, %.ll, $(BITCODES))
SUBDIRS = pmem

.PHONY: all clean subdir

all: $(BITCODES) $(ASSEMBLYS) subdir

subdir:
	for dir in $(SUBDIRS); do \
		make -C $$dir; \
	done

%.bc: %.c
	clang -c -g -O0 -emit-llvm $< -o $@

%.ll: %.bc
	llvm-dis $<

clean:
	rm -f $(BITCODES) $(ASSEMBLYS)
	for dir in $(SUBDIRS); do \
		make -C $$dir clean; \
	done
