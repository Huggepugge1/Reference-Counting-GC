COMPILER = gcc
CFLAGS = -Wall -pedantic -std=c99 -g

main: src/main.c
	$(COMPILER) $(CFLAGS) -o main src/main.c

.DEFAULT_GOAL := run
run: main
	./main

clean:
	rm -f main
