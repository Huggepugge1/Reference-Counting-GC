COMPILER = gcc
CFLAGS = -Wall -pedantic -std=c99 -g

MEMTESTER = valgrind
MEMFLAGS = --leak-check=full --show-leak-kinds=all

main: ./src/main.c ./src/refmem.h ./src/refmem.c
	$(COMPILER) $(CFLAGS) -o main ./src/main.c ./src/refmem.c

memtest: main
	$(MEMTESTER) $(MEMFLAGS) ./main

.DEFAULT_GOAL := run
run: main
	./main

clean:
	rm -f main
