COMPILER = gcc
CFLAGS = -Wall -pedantic -std=c99 -g

MEMTESTER = valgrind
MEMFLAGS = --leak-check=full --show-leak-kinds=all

bin/main: ./src/main.c ./src/refmem.h ./src/refmem.c
	$(COMPILER) $(CFLAGS) -o ./bin/main ./src/main.c ./src/refmem.c

memtest: ./bin/main
	$(MEMTESTER) $(MEMFLAGS) ./bin/main

.DEFAULT_GOAL := run
run: ./bin/main
	./bin/main

clean:
	rm -rf bin
