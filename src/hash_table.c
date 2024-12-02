#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "hash_table.h"
#include "iterator.h"
#include "linked_list.h"

/**
 * A key-value pair
 *  - key: A unique key
 *  - value: The value associated with key
 */
typedef struct
{
    elem_t key;   // Unique key
    elem_t value; // Value associated with key
} key_value_pair_t;

/**
 * A stucture for a link in a linked list:
 *  - key_value_pair: The entry of the linked list
 *  - next: A pointer to next link in the linked list
 */
typedef struct link link_t;
struct link
{
    key_value_pair_t key_value_pair; // A key-value pair
    link_t *next;                    // Pointer to the next bucket
};

/**
 * A Structure representing a bucket of data.
 * Points to a linked list structure
 */
typedef link_t *bucket_t;

/**
 * A hash table stucture with linked list chaining:
 *  - buckets: An array of pointers to linked lists
 *  - bucket_count: The size of the array of pointers (buckets)
 *  - key_count: The number of unique keys in the hash table
 *  - hash_func: A predicate function that converts a key into type size_t
 *  - key_eq_check: A predicate function that checks for equivalency between two keys.
 *  - value_eq_check: A predicate function that checks for equivalency between two values.
 */
struct hash_table
{
    bucket_t *buckets;                 // An array of pointers to buckets
    size_t bucket_count;               // Bucket count metadata
    size_t key_count;                  // Total unique keys metadata
    ioopm_hash_function *hash_func;    // Function that converts our union it into a size_t (hash)
    ioopm_eq_function *key_eq_check;   // Equivalence function for keys
    ioopm_eq_function *value_eq_check; // Equivalence function for values
};

typedef void iterate_function(link_t *current_link, void *args);

static link_t *create_link(const elem_t key, const elem_t value, link_t *next);
static size_t get_index(const ioopm_hash_table_t *ht, const elem_t key);
static key_value_pair_t *get_key_value_pair(link_t *link_ptr);
static elem_t get_key(link_t *link_ptr);
static elem_t *get_value_ptr(link_t *link_ptr);
static link_t **find_previous_pointer(const ioopm_hash_table_t *ht, link_t **first_link, const elem_t key);
static link_t *get_linked_list_head(const ioopm_hash_table_t *ht, const elem_t key);
static void initialize_hash_table(ioopm_hash_table_t **ht, bucket_t *bucket_array, const size_t bucket_count,
                                  ioopm_hash_function *hash_func, ioopm_eq_function *key_eq_check,
                                  ioopm_eq_function *value_eq_check);
static void update_value(link_t *link_ptr, void *new_value);
static void update_key_count(ioopm_hash_table_t **ht, const int add);
static int prepend_key(elem_t key, elem_t *value_unusued __attribute__((unused)), void *key_list);
static int prepend_value(elem_t key, elem_t *value_unusued __attribute__((unused)), void *key_list);
static int remove_key_from_table(elem_t key, elem_t *value_unusued __attribute__((unused)), void *ht);
static void get_value(link_t *link_ptr, void *return_value);
static bool apply_to_one(ioopm_hash_table_t *ht, const elem_t key, iterate_function iterate_func, void *arg_iterate);
static void do_nothing(link_t *link_ignored __attribute__((unused)), void *args_ignored __attribute__((unused)));
static void hash_table_resize(ioopm_hash_table_t **ht, ioopm_hash_function *hash_func, ioopm_eq_function *key_eq_check,
                              ioopm_eq_function *value_eq_check);

int ioopm_create_hash_table(ioopm_hash_table_t **return_ht, size_t bucket_count, ioopm_hash_function *hash_func,
                            ioopm_eq_function *key_eq_check, ioopm_eq_function *value_eq_check)
{
    // Check presumptions
    // Checks if the argument pointer is valid, returns an error status
    if (return_ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }
    // Checks if the hash table size is valid, returns an error status
    if (bucket_count == 0)
    {
        return INVALID_BUCKET_COUNT;
    }

    // Allocate memory for hash table
    *return_ht = malloc(sizeof(ioopm_hash_table_t));

    // Check if allocation worked
    if (*return_ht == NULL)
    {
        // Allocation failed
        // Report error to user
        return MEMORY_ALLOCATION_FAILURE;
    }

    bucket_t *bucket_array = calloc(bucket_count, sizeof(bucket_t));

    // Check if allocation worked
    if (bucket_array == NULL)
    {
        // Allocation failed
        // Free hash_table_t pointer or memory leek will occur
        free(*return_ht);
        // Report error to user
        return MEMORY_ALLOCATION_FAILURE;
    }

    // Initialize the hash table's fields
    initialize_hash_table(return_ht, bucket_array, bucket_count, hash_func, key_eq_check, value_eq_check);

    return SUCCESS;
}

int ioopm_hash_table_insert(ioopm_hash_table_t *ht, const elem_t key, const elem_t value)
{
    // Check presumptions
    // Checks if the argument pointer is valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }

    if (((ht)->key_count / (double)(ht)->bucket_count) > 1.5)
    {
        hash_table_resize(&ht, ht->hash_func, ht->key_eq_check, ht->value_eq_check);
    }

    // Update the key-value pair if the key is already in the hash table
    if (apply_to_one(ht, key, update_value, (void *)&value))
        return SUCCESS;

    // Get head of the correct linked list
    link_t *head = get_linked_list_head(ht, key);

    // Append the key-value pair to the linked list
    link_t *new_head = create_link(key, value, head);

    // Update hash table array to point to the new head
    ht->buckets[get_index(ht, key)] = new_head;

    // Increment hash table metadata for the total number of keys
    update_key_count(&ht, 1);

    return SUCCESS;
}

int ioopm_hash_table_lookup(bool *return_found, elem_t *return_value, const ioopm_hash_table_t *ht, const elem_t key)
{
    // Check presumptions
    // Checks if the argument pointers are valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }
    if (return_found == NULL)
    {
        return INVALID_RETURN_POINTER;
    }
    if (return_value == NULL)
    {
        return INVALID_RETURN_POINTER;
    }

    // Update return_value and return_found to reflect whether the key is in the hash table
    *return_found = (apply_to_one((ioopm_hash_table_t *)ht, key, get_value, (void *)return_value));

    return SUCCESS;
}

int ioopm_hash_table_keys(ioopm_list_t **return_key_list, const ioopm_hash_table_t *ht)
{
    // Check presumptions
    // Checks if the argument pointers are valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }
    if (return_key_list == NULL)
    {
        return INVALID_RETURN_POINTER;
    }

    int status = ioopm_linked_list_create(return_key_list, ht->key_eq_check);
    if (status != SUCCESS)
    {
        return status;
    }

    // Prepend each key to key_list
    ioopm_hash_table_apply_to_all((ioopm_hash_table_t *)ht, prepend_key, *return_key_list);

    return SUCCESS;
}

int ioopm_hash_table_values(ioopm_list_t **return_value_list, const ioopm_hash_table_t *ht)
{
    // Check presumptions
    // Checks if the argument pointers are valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }
    if (return_value_list == NULL)
    {
        return INVALID_RETURN_POINTER;
    }

    int status = ioopm_linked_list_create(return_value_list, ht->value_eq_check);
    if (status != SUCCESS)
    {
        return status;
    }

    // Prepend each value to value_list
    status = ioopm_hash_table_apply_to_all((ioopm_hash_table_t *)ht, prepend_value, *return_value_list);

    return status;
}

int ioopm_hash_table_remove(ioopm_hash_table_t *ht, const elem_t key)
{
    // Check presumptions
    // Checks if the argument pointers are valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }

    link_t **prev = find_previous_pointer(ht, &(ht->buckets[get_index(ht, key)]), key);

    // if prev ptr is NULL, the key does not exist in this bucket
    if (*prev)
    {
        // remove link
        link_t *to_unlink = *prev;
        *prev = to_unlink->next;
        free(to_unlink);

        // Update hash table key_count metadata
        update_key_count(&ht, -1);
    }

    return SUCCESS;
}

int ioopm_hash_table_destroy(ioopm_hash_table_t **ht)
{
    // Check presumptions
    // Checks if the argument pointer is valid, returns an error status
    if (ht == NULL || *ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }
    // clear buckets from links
    ioopm_hash_table_clear(*ht);

    // Free buckets
    free((*ht)->buckets);

    // Free hash table
    free(*ht);

    // safe guard no futher usage of old deallocated pointer
    *ht = NULL;
    return SUCCESS;
}

int ioopm_hash_table_has_key(bool *return_found, const ioopm_hash_table_t *ht, const elem_t key)
{
    // Check presumptions
    // Checks if the argument pointers are valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }
    if (return_found == NULL)
    {
        return INVALID_RETURN_POINTER;
    }

    // Check if key exists
    *return_found = apply_to_one((ioopm_hash_table_t *)ht, key, do_nothing, NULL);

    return SUCCESS;
}

int ioopm_hash_table_has_value(bool *return_found, const ioopm_hash_table_t *ht, const elem_t value)
{
    // Check presumptions
    // Checks if the argument pointers are valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }
    if (return_found == NULL)
    {
        return INVALID_RETURN_POINTER;
    }

    // Check each bucket
    for (size_t bucket_index = 0; bucket_index < ht->bucket_count; bucket_index++)
    {
        // Linearly search linked list until the value is found
        link_t *link_ptr = ht->buckets[bucket_index];
        while (link_ptr != NULL)
        {
            if (ht->value_eq_check(*get_value_ptr(link_ptr), value))
            {
                *return_found = true; // Value found
                return SUCCESS;
            }
            link_ptr = link_ptr->next; // Go to next link
        }
    }
    *return_found = false; // Value not found

    return SUCCESS;
}

int ioopm_hash_table_size(size_t *return_size, const ioopm_hash_table_t *ht)
{
    // Check presumptions
    // Checks if the argument pointers are valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }
    if (return_size == NULL)
    {
        return INVALID_RETURN_POINTER;
    }

    *return_size = ht->key_count;

    return SUCCESS;
}

int ioopm_hash_table_is_empty(bool *return_empty, const ioopm_hash_table_t *ht)
{
    // Check presumptions
    // Checks if the argument pointers are valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }
    if (return_empty == NULL)
    {
        return INVALID_RETURN_POINTER;
    }

    *return_empty = ht->key_count == 0;

    return SUCCESS;
}

int ioopm_hash_table_clear(ioopm_hash_table_t *ht)
{
    // Check presumptions
    // Checks if the argument pointer is valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }

    // Iterate through each key, removing it from the hash table
    int status = SUCCESS;
    status = ioopm_hash_table_apply_to_all(ht, remove_key_from_table, ht);

    return status;
}

int ioopm_hash_table_all(bool *return_all, const ioopm_hash_table_t *ht, ioopm_hash_table_predicate *pred,
                         const void *arg)
{
    // Check presumptions
    // Checks if the argument pointers are valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }
    if (return_all == NULL)
    {
        return INVALID_RETURN_POINTER;
    }

    ioopm_list_t *key_list = NULL;
    ioopm_hash_table_keys(&key_list, ht);

    ioopm_list_iterator_t *key_iter = NULL;
    ioopm_list_iterator(&key_iter, key_list);

    ioopm_list_t *value_list = NULL;
    ioopm_hash_table_values(&value_list, ht);

    ioopm_list_iterator_t *value_iter = NULL;
    ioopm_list_iterator(&value_iter, value_list);

    bool result = true;
    bool has_next_key = false;
    bool has_next_value = false;
    elem_t current_key;
    elem_t current_value;
    while (ioopm_iterator_has_next(&has_next_key, key_iter) == SUCCESS && has_next_key &&
           ioopm_iterator_has_next(&has_next_value, value_iter) == SUCCESS && has_next_value)
    {
        ioopm_iterator_next(&current_key, key_iter);
        ioopm_iterator_next(&current_value, value_iter);
        result = result && pred(current_key, current_value, arg);
    }

    ioopm_iterator_destroy(&value_iter);
    ioopm_iterator_destroy(&key_iter);
    ioopm_linked_list_destroy(&value_list);
    ioopm_linked_list_destroy(&key_list);

    *return_all = result;

    return SUCCESS;
}

int ioopm_hash_table_any(bool *return_any, const ioopm_hash_table_t *ht, ioopm_hash_table_predicate *pred,
                         const void *arg)
{
    // Check presumptions
    // Checks if the argument pointers are valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }
    if (return_any == NULL)
    {
        return INVALID_RETURN_POINTER;
    }

    ioopm_list_t *key_list = NULL;
    ioopm_hash_table_keys(&key_list, ht);

    ioopm_list_iterator_t *key_iter = NULL;
    ioopm_list_iterator(&key_iter, key_list);

    ioopm_list_t *value_list = NULL;
    ioopm_hash_table_values(&value_list, ht);

    ioopm_list_iterator_t *value_iter = NULL;
    ioopm_list_iterator(&value_iter, value_list);

    bool result = false;
    bool has_next_key = false;
    bool has_next_value = false;
    elem_t current_key;
    elem_t current_value;
    while (ioopm_iterator_has_next(&has_next_key, key_iter) == SUCCESS && has_next_key &&
           ioopm_iterator_has_next(&has_next_value, value_iter) == SUCCESS && has_next_value)
    {
        ioopm_iterator_next(&current_key, key_iter);
        ioopm_iterator_next(&current_value, value_iter);
        result = result || pred(current_key, current_value, arg);
    }

    ioopm_iterator_destroy(&value_iter);
    ioopm_iterator_destroy(&key_iter);
    ioopm_linked_list_destroy(&value_list);
    ioopm_linked_list_destroy(&key_list);

    *return_any = result;

    return SUCCESS;
}

int ioopm_hash_table_apply_to_all(ioopm_hash_table_t *ht, ioopm_hash_table_apply_function *apply_fun, void *arg)
{
    // Check presumptions
    // Checks if the argument pointer is valid, returns an error status
    if (ht == NULL)
    {
        return INVALID_HASH_TABLE_POINTER;
    }

    // Iterate through all non empty links
    for (size_t bucket_index = 0; bucket_index < ht->bucket_count; bucket_index++)
    {
        link_t *link_ptr = ht->buckets[bucket_index];
        while (link_ptr != NULL)
        {
            link_t *next_link = link_ptr->next;
            apply_fun(get_key(link_ptr), get_value_ptr(link_ptr), arg);
            link_ptr = next_link; // Go to next link
        }
    }

    return SUCCESS;
}

/**
 * @brief Updates the value stored in the link.
 * @param link_ptr Pointer to the link whose value will be updated.
 * @param new_value The new value.
 */
static void update_value(link_t *link_ptr, void *new_value)
{
    get_key_value_pair(link_ptr)->value = *((elem_t *)new_value);
}

/**
 * @brief Modifies the size of the hash table by adding an integer.
 * @param ht A pointer to a hash table.
 * @param add Is added to the hash table's size.
 */
static void update_key_count(ioopm_hash_table_t **ht, int add)
{
    (*ht)->key_count += add;
}

/**
 * @brief Returns a link's value using the return_value parameter.
 * @param link_ptr the link pointer to get value from
 * @param return_value used to return a value to the caller
 */
static void get_value(link_t *link_ptr, void *return_value)
{
    *((elem_t *)return_value) = *get_value_ptr(link_ptr);
}

/**
 * @brief Finds a given key and calls a function on the associated link.
 * @param ht The hash table.
 * @param key The key assiciated with the link on which the function is called.
 * @param iterate_func A function pointer that is applied to a link specified by the key.
 * @param extra Any extra arguments for the iterate function.
 * @param returns Returns true if the key was found, and by extension if the iterate func was called. Otherwise, false.
 */
static bool apply_to_one(ioopm_hash_table_t *ht, elem_t key, iterate_function iterate_func, void *extra)
{
    link_t *link = get_linked_list_head(ht, key);
    while (link != NULL)
    {
        if (ht->key_eq_check(get_key(link), key))
        {
            iterate_func(link, extra); // Apply iterate func
            return true;
        }
        link = link->next; // Go to next link
    }
    return false;
}

/**
 * @brief Prepends a key to a list of keys.
 * @param key The key to prepend.
 * @param value_unusued NOT USED IN FUNCTION
 * @param key_list A pointer to the list we want to prepend the key to.
 * @return The exit status of the function:
 *  - SUCCESS:                      hash table was successfully created
 *  - INVALID_LIST_POINTER:         key_list is NULL (invalid pointer)
 * @note key_list is modified in this function
 */
static int prepend_key(elem_t key, elem_t *value_unusued __attribute__((unused)), void *key_list)
{
    int status = SUCCESS;
    status = ioopm_linked_list_prepend((ioopm_list_t *)key_list, key);
    return status;
}

/**
 * @brief Prepends a value to a list of values.
 * @param value The value that is prepended.
 * @param value_list A pointer to the last element in an array of values.
 * @note link_ptr is modified in this function
 */
static int prepend_value(elem_t key_unusued __attribute__((unused)), elem_t *value, void *value_list)
{
    int status = SUCCESS;
    status = ioopm_linked_list_prepend((ioopm_list_t *)value_list, *value);
    return status;
}

/**
 * @brief Removes a key from the hash table.
 * @param key The key to be removed.
 * @param value_unusued NOT USED IN FUNCTION
 * @param ht  The hash table we want to remove the key from.
 * @return the exit status of the function:
 *  - SUCCESS:                      remove successful
 *  - INVALID_HASH_TABLE_POINTER:   ht is NULL (invalid pointer)
 * @note ht is modified in this function
 */
static int remove_key_from_table(elem_t key, elem_t *value_unusued __attribute__((unused)), void *ht)
{
    int status = SUCCESS;
    status = ioopm_hash_table_remove(((ioopm_hash_table_t *)ht), key);
    return status;
}

/**
 * @brief Does nothing and is of type iterate_func.
 */
static void do_nothing(link_t *link_ignored __attribute__((unused)), void *args_ignored __attribute__((unused)))
{
    // Do nothing
}

/**
 * @brief Gets a pointer to a linked list head that matches to a given keys hash function.
 * @param ht A hash table.
 * @param key A unique key.
 * @return A pointer to the head of a linked list or NULL.
 */
static link_t *get_linked_list_head(const ioopm_hash_table_t *ht, const elem_t key)
{
    return ht->buckets[get_index(ht, key)];
}

/**
 * @brief Returns a pointer to an initialized link stored on the heap.
 * @param key The key to find this value.
 * @param value The value this link contains.
 * @param next A pointer to the next link.
 * @return A pointer to the newly created link.
 */
static link_t *create_link(const elem_t key, const elem_t value, link_t *next)
{
    link_t *new_link = malloc(1 * sizeof(link_t));
    *new_link = (link_t){.key_value_pair.key = key, .key_value_pair.value = value, .next = next};
    return new_link;
}

/**
 * @brief Uses hash function on elem_t.
 * @param ht The hash table in which we want to hash elem_t into.
 * @param key A unique key.
 * @return The hash of the unique key.
 */
static size_t get_hash(const ioopm_hash_table_t *ht, const elem_t key)
{
    if (ht->hash_func == NULL)
    {
        // treat keys as integers
        return (size_t)key.integer; // .integer reads the integer part of the elem_t
    }
    else
    {
        return (size_t)ht->hash_func(key);
    }
}

/**
 * @brief Uses modular hashing to find the index of an pair.
 *        The hash function is h(k) = k mod t, where k is the key and t is the number of possible indexes.
 * @param ht The hash table to get number of possible indexes and hash function from.
 * @param key A unique key.
 * @return The index of where the unique key should be stored in the hash table array.
 */
static size_t get_index(const ioopm_hash_table_t *ht, const elem_t key)
{
    return get_hash(ht, key) % ht->bucket_count;
}

/**
 * @brief Returns the pointer to a key-value pair inside of a link.
 * @param link_ptr The link pointer to get key-value pair from.
 * @return A pointer to a key-value pair in a given link.
 */
static key_value_pair_t *get_key_value_pair(link_t *link_ptr)
{
    assert(link_ptr != NULL);
    return &(link_ptr->key_value_pair);
}

/**
 * @brief Returns a given link's key.
 * @param link_ptr The link pointer to get key from.
 * @return A link's key.
 */
static elem_t get_key(link_t *link_ptr)
{
    return get_key_value_pair(link_ptr)->key;
}

/**
 * @brief Returns a reference to the value stored in a given link.
 * @param link_ptr The link pointer to get value from.
 * @return A pointer to the value in the link.
 */
static elem_t *get_value_ptr(link_t *link_ptr)
{
    return &(get_key_value_pair(link_ptr)->value);
}

/**
 * @brief Gets a pointer to the pointer to the link before a given key.
 * @param first_link A pointer to a pointer to the head of a linked list.
 * @return A pointer to a pointer that points to the link before a given key.
 */
static link_t **find_previous_pointer(const ioopm_hash_table_t *ht, link_t **first_link, const elem_t key)
{
    // pointer to a pointer to the head of the linked list
    link_t **next_link_ptr = first_link;

    // While linked list exists and not reached end of linked list
    while (*next_link_ptr)
    {
        // link we point to is not null
        if (ht->key_eq_check((*next_link_ptr)->key_value_pair.key, key))
        {
            // Points to link with matching key,
            // Return a pointer to the link_t pointer
            return next_link_ptr;
        }
        else
        {
            // Test next link
            // Update our Pointer to the next link_t pointer
            next_link_ptr = &((*next_link_ptr)->next);
        }
    }
    // Key does not exist in this bucket, return pointer to pointer to head of linked list
    return first_link;
}

/**
 * @brief Initializes a hash table's fields.
 * @param ht The hash table that will be initialized.
 * @param bucket_array The hash table's array.
 * @param bucket_count The size of the hash table's array.
 * @param hash_func The hashing function to convert elem_t union into size_t.
 * @param key_eq_check Function used when comapring keys against each other.
 * @param value_eq_check Function used when comapring values against each other.
 */
static void initialize_hash_table(ioopm_hash_table_t **ht, bucket_t *bucket_array, size_t bucket_count,
                                  ioopm_hash_function *hash_func, ioopm_eq_function *key_eq_check,
                                  ioopm_eq_function *value_eq_check)
{
    (*ht)->buckets = bucket_array;
    (*ht)->bucket_count = bucket_count;
    (*ht)->key_count = 0;
    (*ht)->hash_func = hash_func;
    (*ht)->key_eq_check = key_eq_check;
    (*ht)->value_eq_check = value_eq_check;
}

void hash_table_resize(ioopm_hash_table_t **ht, ioopm_hash_function *hash_func, ioopm_eq_function *key_eq_check,
                       ioopm_eq_function *value_eq_check)
{
    ioopm_hash_table_t *new_table = NULL;
    ioopm_create_hash_table(&new_table, (size_t)((*ht)->bucket_count * 2), hash_func, key_eq_check, value_eq_check);

    ioopm_list_t *key_list = NULL;
    ioopm_hash_table_keys(&key_list, (ioopm_hash_table_t *)*ht);

    ioopm_list_iterator_t *key_iter = NULL;
    ioopm_list_iterator(&key_iter, key_list);

    size_t key_count = 0;
    ioopm_linked_list_size(&key_count, key_list);

    ioopm_list_t *value_list = NULL;
    ioopm_hash_table_values(&value_list, (ioopm_hash_table_t *)*ht);

    ioopm_list_iterator_t *value_iter = NULL;
    ioopm_list_iterator(&value_iter, value_list);

    elem_t current_key;
    elem_t current_value;
    bool has_next_key = false;
    bool has_next_value = false;
    while (ioopm_iterator_has_next(&has_next_key, key_iter) == SUCCESS && has_next_key &&
           ioopm_iterator_has_next(&has_next_value, value_iter) == SUCCESS && has_next_value)
    {
        ioopm_iterator_next(&current_key, key_iter);
        ioopm_iterator_next(&current_value, value_iter);
        ioopm_hash_table_insert(new_table, current_key, current_value);
    }

    ioopm_iterator_destroy(&value_iter);
    ioopm_iterator_destroy(&key_iter);
    ioopm_linked_list_destroy(&value_list);
    ioopm_linked_list_destroy(&key_list);
    ioopm_hash_table_clear((*ht));

    (*ht)->bucket_count = new_table->bucket_count;
    (*ht)->key_count = new_table->key_count;
    free((*ht)->buckets);
    (*ht)->buckets = new_table->buckets;
    free(new_table);
}