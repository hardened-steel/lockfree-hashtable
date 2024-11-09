#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    size_t table_size;
    size_t key_size;
    size_t val_size;
} lockfree_hashtable_config_t;

typedef struct {
    const lockfree_hashtable_config_t* config;
    void* stats;
    void* entries;
    void* keys;
    void* vals;
    void* pool;
} lockfree_hashtable_t;

#ifdef __cplusplus
extern "C" {
#endif

size_t lockfree_hashtable_calc_mem_size(const lockfree_hashtable_config_t* config);

void lockfree_hashtable_init(lockfree_hashtable_t* table, const lockfree_hashtable_config_t* config, void* memory);
bool lockfree_hashtable_insert(lockfree_hashtable_t* table, const void* key, const void* val);
bool lockfree_hashtable_find(lockfree_hashtable_t* table, const void* key, void* val);
void lockfree_hashtable_fini(lockfree_hashtable_t* table);

#ifdef __cplusplus
}
#endif
