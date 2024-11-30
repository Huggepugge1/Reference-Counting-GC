#include "refmem.h"
#include <stdio.h>
#include <stdlib.h>

#define GC_DEFAULT_LIMIT 10000

struct object {
    size_t rc;
    size_t position;
    size_t size;

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
    obj *data = gc->objects[index].data;
    while (object != data) {
        data = gc->objects[++index].data;
    }
    return &gc->objects[index];
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

obj *allocate(size_t bytes, function1_t destructor) {
    if (!gc) {
        gc = create_gc();
    }
    gc->objects = realloc(gc->objects, (gc->count + 1) * sizeof(object_t));

    gc->objects[gc->count].rc = 0;
    gc->objects[gc->count].position = gc->count;
    gc->objects[gc->count].size = bytes;
    gc->objects[gc->count].destructor = destructor;
    gc->objects[gc->count].data = calloc(1, bytes);

    gc->count++;

    return gc->objects[gc->count - 1].data;
}

obj *allocate_array(size_t elements, size_t elem_size, function1_t destructor) {
    if (!gc) {
        gc = create_gc();
    }
    return allocate(elements * elem_size, destructor);
}

void deallocate(obj *data) {
    object_t *object = get_object(data);
    size_t position = object->position + 1;

    object->destructor(data);

    object = get_object(data);
    free(object->data);

    while (position < gc->count - 1) {
        gc->objects[position] = gc->objects[position + 1];
        gc->objects[position].position = position;
        position++;
    }

    gc->count--;
    if (gc->count > 0) {
        gc->objects = realloc(gc->objects, (gc->count) * sizeof(object_t));
    } else {
        free(gc->objects);
        gc->objects = NULL;
    }
}

void set_cascade_limit(size_t limit) { gc->limit = limit; }

size_t get_cascade_limit() { return gc->limit; }

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
        deallocate(gc->objects[0].data);
    }
    free(gc);
}
