#pragma once
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    size_t table_size;
    size_t key_size;
    size_t val_size;
} lockfree_hashtable_config_t;

typedef struct {
    const lockfree_hashtable_config_t* config;
    void* entries;
    void* keys;
    void* vals;
    void* pool;
} lockfree_hashtable_t;

#ifdef __cplusplus
extern "C" {
#endif

// calculate needed size of hash table memory
size_t lockfree_hashtable_calc_mem_size(const lockfree_hashtable_config_t* config);

// init hash table, no additional allocates, no thread safe
void lockfree_hashtable_init(lockfree_hashtable_t* table, const lockfree_hashtable_config_t* config, void* memory);

// insert a new entry to the table, return true if success, return false if the table is full
bool lockfree_hashtable_insert(lockfree_hashtable_t* table, const void* key, const void* val);

// find an entry by key, return true if entry is preset in table, false otherwise
// if "val" is NULL, no value will be copied, just return true if entry is preset
bool lockfree_hashtable_find(lockfree_hashtable_t* table, const void* key, void* val);

// remove an entry by key from hash table, return true if entry was deleted, false if entry not found
bool lockfree_hashtable_erase(lockfree_hashtable_t* table, const void* key);

#ifdef __cplusplus
}
#endif
