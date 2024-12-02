#pragma once
#include <stdbool.h>

typedef union elem elem_t;

union elem {
    int integer;
    unsigned int unsigned_integer;
    bool boolean;
    float floating_point;
    void *unknown_pointer;
};

#define int_elem(x)                                                                                                    \
    (elem_t)                                                                                                           \
    {                                                                                                                  \
        .integer = (x)                                                                                                 \
    }
#define ptr_elem(x)                                                                                                    \
    (elem_t)                                                                                                           \
    {                                                                                                                  \
        .unknown_pointer = (x)                                                                                         \
    }
#define bool_elem(x)                                                                                                   \
    (elem_t)                                                                                                           \
    {                                                                                                                  \
        .boolean = (x)                                                                                                 \
    }
#define float_elem(x)                                                                                                  \
    (elem_t)                                                                                                           \
    {                                                                                                                  \
        .floating_point = (x)                                                                                          \
    }
#define uint_elem(x)                                                                                                   \
    (elem_t)                                                                                                           \
    {                                                                                                                  \
        .unsigned_integer = (x)                                                                                        \
    }

#define SUCCESS 0
#define UNSPECIFIED_FAILURE 1
#define MEMORY_ALLOCATION_FAILURE 2
#define INVALID_RETURN_POINTER 3

typedef bool ioopm_eq_function(elem_t a, elem_t b);