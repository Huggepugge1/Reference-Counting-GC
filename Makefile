COMPILER = gcc
CFLAGS = -Wall -pedantic -std=c99 -g

MEMTESTER = valgrind
MEMFLAGS = --leak-check=full --show-leak-kinds=all

main: ./src/main.c ./src/refmem.h ./src/refmem.c
	$(COMPILER) $(CFLAGS) -o ./bin/main ./src/main.c ./src/refmem.c

memtest: main
	$(MEMTESTER) $(MEMFLAGS) ./main

.DEFAULT_GOAL := run
run: ./bin/main
	./bin/main

clean:
	rm -f main
