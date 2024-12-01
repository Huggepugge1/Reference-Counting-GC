#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef void obj;
typedef void (*function1_t)(obj *);

/**
 * @brief Retain an object.
 * @details Increment the reference count of an object.
 * @param obj The object to retain.
 */
void retain(obj *);

/**
 * @brief Release an object.
 * @details Decrement the reference count of an object. If the reference count
 * is 0, the object is deallocated.
 * @param obj The object to release.
 */
void release(obj *);

/**
 * @brief Get the reference count of an object.
 *
 * @param obj The object to get the reference count of.
 * @return size_t The reference count of the object.
 */
size_t rc(obj *);

/**
 * @brief Allocate an object.
 *
 * @param bytes The size of the object to allocate.
 * @param destructor The destructor function to call when the object is
 * deallocated.
 * @param pointers The number of pointers to heap-allocated objects in the
 * object.
 * @return obj* The allocated object.
 *
 * @precodition All pointers in the object to other heap-allocated objects must
 * be at the beginning of the object.
 */
obj *allocate(size_t bytes, function1_t destructor, size_t pointers);

/**
 * @brief Allocate an array of objects.
 *
 * @param elements The number of elements in the array.
 * @param elem_size The size of each element in the array.
 * @param destructor The destructor function to call when the object is
 * deallocated.
 * @param pointers The number of pointers to heap-allocated objects in each of
 * the elements in the array. The pointers must be at the beginning of each
 * element.
 * @return obj* The allocated array.
 */
obj *allocate_array(size_t elements, size_t elem_size, function1_t destructor,
                    size_t pointers);

/**
 * @brief Deallocate an object.
 *
 * @param object The object to deallocate.
 */
void deallocate(obj *);

/**
 * @brief Set the cascade limit.
 *
 * @param limit The new cascade limit.
 */
void set_cascade_limit(size_t);

/**
 * @brief Get the cascade limit.
 *
 * @return size_t The current cascade limit.
 */
size_t get_cascade_limit();

/**
 * @brief Get the number of objects in the garbage collector.
 *
 * @return size_t The number of objects in the garbage collector.
 */
size_t get_count();

/**
 * @brief Clean up all allocated memory.
 */
void cleanup();

/**
 * @brief Shut down the garbage collector.
 */
void shutdown();
