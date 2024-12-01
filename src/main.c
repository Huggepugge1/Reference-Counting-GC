#include "refmem.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct cell {
    struct cell *cell;
    char *string;
    int i;
};

void test_array() {
    int **array = allocate_array(10, sizeof(int *), NULL, 1);
    retain(array);

    for (int i = 0; i < 10; i++) {
        array[i] = allocate(sizeof(int), NULL, 0);
        retain(array[i]);
        *array[i] = i;
    }
    release(array);
}

int main(void) {
    struct cell *c = allocate(sizeof(struct cell), NULL, 2);
    retain(c);

    retain(c->string);

    c->cell = allocate(sizeof(struct cell), NULL, 1);
    c->cell->i = 42;
    retain(c->cell);
    c->cell = NULL;

    release(c->cell);
    c->cell = allocate(sizeof(struct cell), NULL, 1);
    c->cell->i = 40;
    retain(c->cell);

    c->cell->cell = NULL;

    test_array();
    release(c);

    shutdown();

    return 0;
}
