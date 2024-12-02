#include "refmem.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GC_DEFAULT_LIMIT 10000
#define PAGE_SIZE 1024

void default_destructor(obj *data);
void default_array_destructor(obj *data);

struct object {
    obj *data;

    int32_t rc;

    int8_t pointers;
    uint8_t destructor;

    bool array;
};

typedef struct object object_t;

struct gc {
    size_t count;
    size_t limit;
    size_t cascade_count;

    object_t *objects;
    size_t objects_size;

    function1_t *destructors;
    size_t destructor_count;

    size_t reallocations;
};

struct gc *gc = NULL;

bool obj_is_object(obj *data, object_t *object) {
    if (object->array) {
        return data == object->data + 2 * sizeof(size_t);
    }
    return data == object->data;
}

object_t *get_object(obj *object) {
    size_t index = 0;
    obj *data = NULL;
    while (!obj_is_object(object, &gc->objects[index])) {
        index++;
    }
    return &gc->objects[index];
}

size_t get_object_position(obj *object) {
    size_t index = 0;
    obj *data = NULL;
    while (!obj_is_object(object, &gc->objects[index])) {
        index++;
    }
    return index;
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
    gc->objects = malloc(sizeof(object_t) * 1024);
    gc->objects_size = 1024;
    gc->count = 0;

    gc->limit = GC_DEFAULT_LIMIT;
    gc->cascade_count = 0;

    gc->destructors = malloc(3 * sizeof(function1_t));
    gc->destructors[0] = NULL;
    gc->destructors[1] = default_destructor;
    gc->destructors[2] = default_array_destructor;
    gc->destructor_count = 2;

    gc->reallocations = 0;

    return gc;
}

size_t get_destructor_index(function1_t destructor) {
    size_t index = 0;
    while (index < gc->destructor_count) {
        if (gc->destructors[index] == destructor) {
            return index;
        }
        index++;
    }
    gc->destructors =
        realloc(gc->destructors, (index + 1) * sizeof(function1_t));
    gc->destructors[index] = destructor;
    gc->destructor_count++;
    return index;
}

void allocate_space() {
    if (gc->count == gc->objects_size) {
        gc->objects_size += PAGE_SIZE;
        gc->objects = realloc(gc->objects, gc->objects_size * sizeof(object_t));
        gc->reallocations++;
    }
}

void default_destructor(obj *data) {
    object_t *object = get_object(data);
    size_t pointer_index = 0;
    while (pointer_index < object->pointers) {
        release(((void **)data)[pointer_index]);
        pointer_index++;
        object = get_object(data);
    }
}

void initialize_object(size_t bytes, function1_t destructor, size_t pointers) {
    object_t *object = &gc->objects[gc->count];

    object->rc = 0;
    object->destructor = get_destructor_index(destructor);
    object->pointers = pointers;
    object->array = false;
    object->data = calloc(1, bytes);
}

obj *allocate(size_t bytes, function1_t destructor, size_t pointers) {
    if (!gc) {
        gc = create_gc();
    }

    if (destructor == NULL) {
        destructor = default_destructor;
    }

    allocate_space();

    initialize_object(bytes, destructor, pointers);
    gc->count++;

    return gc->objects[gc->count - 1].data;
}

void default_array_destructor(obj *data) {
    object_t *object = get_object(data);
    size_t elements = *(size_t *)(data - sizeof(size_t) * 2);
    size_t object_size = *(size_t *)(data - sizeof(size_t));
    for (int element = 0; element < elements; element++) {
        size_t pointer_index = 0;
        while (pointer_index < object->pointers) {
            release(((void **)(data + object_size * element))[pointer_index]);
            pointer_index++;
            object = get_object(data);
        }
    }
}

void initialize_array_object(size_t elements, size_t bytes,
                             function1_t destructor, size_t pointers) {
    object_t *object = &gc->objects[gc->count];

    object->rc = 0;
    object->destructor = get_destructor_index(destructor);
    object->pointers = pointers;
    object->array = true;
    object->data = calloc(1, elements * bytes + 2 * sizeof(size_t));
    *(size_t *)object->data = elements;
    *(size_t *)(object->data + sizeof(size_t)) = bytes;
}

obj *allocate_array(size_t elements, size_t elem_size, function1_t destructor,
                    size_t pointers) {
    if (!gc) {
        gc = create_gc();
    }

    if (destructor == NULL) {
        destructor = default_array_destructor;
    }

    allocate_space();

    initialize_array_object(elements, elem_size, destructor, pointers);
    gc->count++;

    return gc->objects[gc->count - 1].data + 2 * sizeof(size_t);
}

void deallocate(obj *data) {
    size_t position = get_object_position(data);
    object_t *object = &gc->objects[position];

    if (object->destructor) {
        gc->destructors[object->destructor](data);
    }

    object = get_object(data);
    free(object->data);

    gc->count--;

    while (position < gc->count) {
        gc->objects[position] = gc->objects[position + 1];
        position++;
    }

    if (gc->objects_size - gc->count > PAGE_SIZE) {
        gc->objects_size -= PAGE_SIZE;
        gc->objects = realloc(gc->objects, gc->objects_size * sizeof(object_t));
        gc->reallocations++;
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
        gc->objects[0].destructor = 0;
        deallocate(gc->objects[0].data);
    }
    printf("Reallocation count: %lu\n", gc->reallocations);
    free(gc->objects);
    free(gc->destructors);
    free(gc);
}
