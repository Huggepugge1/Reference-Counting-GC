#include "refmem.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define GC_DEFAULT_LIMIT 10000

struct object {
    size_t rc;
    size_t size;
    size_t array_size;
    size_t pointers;

    function1_t destructor;

    obj *data;
};

typedef struct object object_t;

struct gc {
    size_t count;
    size_t limit;
    size_t cascade_count;

    object_t *objects;
};

struct gc *gc = NULL;

object_t *get_object(obj *object) {
    size_t index = 0;
    obj *data = NULL;
    while (object != data && index < gc->count) {
        data = gc->objects[index].data;
        index++;
    }
    if (index - 1 < gc->count) {
        return &gc->objects[index - 1];
    }
    return NULL;
}

size_t get_object_position(obj *object) {
    size_t index = 0;
    obj *data = NULL;
    while (object != data && index < gc->count) {
        data = gc->objects[index].data;
        index++;
    }
    return index - 1;
}

void retain(obj *data) {
    if (!data) {
        return;
    }
    object_t *object = get_object(data);
    object->rc++;
    gc->cascade_count = 0;
}

void release(obj *data) {
    if (!data) {
        return;
    }
    object_t *object = get_object(data);
    object->rc--;
    if (object->rc == 0) {
        if (gc->limit > 0 && gc->cascade_count < gc->limit) {
            deallocate(data);
            gc->cascade_count++;
        }
    }
}

size_t rc(obj *data) {
    object_t *object = get_object(data);
    return object->rc;
}

struct gc *create_gc() {
    struct gc *gc = malloc(sizeof(struct gc));
    gc->objects = NULL;
    gc->count = 0;
    gc->limit = GC_DEFAULT_LIMIT;
    gc->cascade_count = 0;
    return gc;
}

void default_destructor(obj *data) {
    object_t *object = get_object(data);
    for (int element = 0; element < object->array_size; element++) {
        size_t pointer_index = 0;
        while (pointer_index < object->pointers) {
            release(((void **)(data + object->size * element))[pointer_index]);
            pointer_index++;
            object = get_object(data);
        }
    }
}

void initialize_object(size_t elements, size_t bytes, function1_t destructor,
                       size_t pointers) {
    object_t *object = &gc->objects[gc->count];

    object->rc = 0;
    object->size = bytes;
    object->array_size = elements;
    object->destructor = destructor;
    object->pointers = pointers;
    object->data = calloc(elements, bytes);
}

obj *allocate(size_t bytes, function1_t destructor, size_t pointers) {
    return allocate_array(1, bytes, destructor, pointers);
}

obj *allocate_array(size_t elements, size_t elem_size, function1_t destructor,
                    size_t pointers) {
    if (!gc) {
        gc = create_gc();
    }

    if (destructor == NULL) {
        destructor = default_destructor;
    }

    if (gc->count > 0) {
        gc->objects = realloc(gc->objects, (gc->count + 1) * sizeof(object_t));
    } else {
        gc->objects = calloc(1, sizeof(object_t));
    }

    initialize_object(elements, elem_size, destructor, pointers);
    gc->count++;

    return gc->objects[gc->count - 1].data;
}

void deallocate(obj *data) {
    size_t position = get_object_position(data);
    object_t *object = &gc->objects[position];

    if (object->destructor) {
        object->destructor(data);
    }

    object = get_object(data);
    free(object->data);

    while (position < gc->count - 1) {
        gc->objects[position] = gc->objects[position + 1];
        position++;
    }

    gc->count--;
    if (gc->count > 0) {
        gc->objects = realloc(gc->objects, (gc->count) * sizeof(object_t));
    } else {
        free(gc->objects);
    }
}

void set_cascade_limit(size_t limit) { gc->limit = limit; }

size_t get_cascade_limit() { return gc->limit; }

size_t get_count() { return gc->count; }

void cleanup() {
    if (!gc->objects) {
        return;
    }
    size_t index = 0;
    while (index < gc->count) {
        if (gc->objects[index].rc == 0) {
            deallocate(gc->objects[index].data);
        }
        index++;
    }
}

void shutdown() {
    while (gc->count > 0) {
        gc->objects[0].destructor = NULL;
        deallocate(gc->objects[0].data);
    }
    free(gc);
}
