#include "lockfree-hashtable.h"
#include <limits.h>
#include <string.h>
#include <stdatomic.h>

typedef _Atomic(uint32_t) atomic_uint32_t;
typedef _Atomic(uint64_t) atomic_uint64_t;

#define NULL_ITEM UINT32_MAX

static size_t roundup(size_t x, size_t y) {
    return ((x + y - 1) / y) * y;
}

static uint32_t calc_hash(const void *data, size_t size)
{
    const uint8_t *p = data;
    uint32_t h = 0;

    while(size--) {
        h += *p++;
        h += (h << 10);
        h ^= (h >> 6);
    }

    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);

    return h;
}

size_t lockfree_hashtable_calc_mem_size(const lockfree_hashtable_config_t* config)
{
    const size_t table_size = config->table_size * sizeof(atomic_uint64_t);
    const size_t keys       = roundup(config->table_size * config->key_size, sizeof(uint64_t));
    const size_t values     = roundup(config->table_size * config->val_size, sizeof(uint64_t));
    const size_t pool_size  = config->table_size / 64u + (config->table_size % 64u ? 1 : 0);
    const size_t pool       = pool_size * sizeof(atomic_uint64_t);
    return table_size + keys + values + pool;
}

void lockfree_hashtable_init(lockfree_hashtable_t* table, const lockfree_hashtable_config_t* config, void* memory)
{
    table->config = config;

    const size_t table_size = config->table_size * sizeof(atomic_uint64_t);
    const size_t key_size   = roundup(config->table_size * config->key_size, sizeof(uint64_t));
    const size_t val_size   = roundup(config->table_size * config->val_size, sizeof(uint64_t));
    const size_t pool_size  = config->table_size / 64u + (config->table_size % 64u ? 1 : 0);

    uint8_t *ptr = memory;
    size_t offset = 0;

    table->entries = ptr + offset;
    offset += table_size;

    table->keys = ptr + offset;
    offset += key_size;

    table->vals = ptr + offset;
    offset += val_size;

    table->pool = ptr + offset;

    memset(table->entries, 0, table_size);
    memset(table->pool, 0, pool_size * sizeof(atomic_uint64_t));
}

static uint32_t allocate_item(lockfree_hashtable_t* table)
{
    const lockfree_hashtable_config_t* config = table->config;
    const size_t pool_size  = config->table_size / 64u + (config->table_size % 64u ? 1 : 0);
    atomic_uint64_t* pool = table->pool;

    // scan and search a free bit in the pool
    for (size_t i = 0; i < pool_size; ++i) {
        const uint64_t chunk = atomic_load_explicit(&pool[i], memory_order_relaxed);
        if (chunk != UINT64_MAX) {
            for (size_t index = 0u; index < 64u; ++index) {
                if (index + i * 64u < config->table_size) {
                    const uint64_t bit = (UINT64_C(1) << index);
                    if ((chunk & bit) == UINT64_C(0)) {
                        // try to set bit
                        if (!(atomic_fetch_or_explicit(&pool[i], bit, memory_order_acquire) & bit)) {
                            return index + i * 64u;
                        }
                    }
                } else {
                    return NULL_ITEM;
                }
            }
        }
    }
    return NULL_ITEM;
}

static void delete_item(lockfree_hashtable_t* table, uint32_t item)
{
    if (item == NULL_ITEM) {
        return;
    }
    atomic_uint64_t* pool = table->pool;
    const uint64_t bit = (UINT64_C(1) << (item % 64));
    atomic_fetch_and_explicit(&pool[item / 64], ~bit, memory_order_release);
}

static void* get_item_key(lockfree_hashtable_t* table, uint32_t item)
{
    const lockfree_hashtable_config_t* config = table->config;
    uint8_t* keys = table->keys;
    return keys + item * config->key_size;
}

static void* get_item_val(lockfree_hashtable_t* table, uint32_t item)
{
    const lockfree_hashtable_config_t* config = table->config;
    uint8_t* vals = table->vals;
    return vals + item * config->val_size;
}

bool lockfree_hashtable_insert(lockfree_hashtable_t* table, const void* key, const void* val)
{
    const lockfree_hashtable_config_t* config = table->config;
    atomic_uint64_t* entries = table->entries;
    atomic_uint64_t* pool = table->pool;

    // allocate a new item
    const uint32_t item = allocate_item(table);
    if (item == NULL_ITEM) {
        return false;
    }

    // fill data from parameters
    memcpy(get_item_key(table, item), key, config->key_size);
    memcpy(get_item_val(table, item), val, config->val_size);

    const uint32_t hash = calc_hash(key, config->key_size);

    for (size_t i = 0, index = hash % config->table_size; i < config->table_size; ++i, index = (index + 1) % config->table_size) {
        // read table entry
        uint64_t old_entry = atomic_load(&entries[index]);
        do {
            const uint32_t old_item = old_entry;
            const uint32_t old_version = old_entry >> 32u;

            uint32_t new_item = item;
            // increment a version
            uint32_t new_version = old_version + 1;
            // calc new entry
            uint64_t new_entry = ((uint64_t)new_version << 32u) | (uint64_t)new_item;

            const bool can_insert = (old_version == 0) // version == 0 means free entry
                // if entry is deleted
                || (old_item == NULL_ITEM)
                // if keys are equal
                || (memcmp(key, get_item_key(table, old_item), config->key_size) == 0)
            ;

            if (can_insert) {
                // try to make a CAS
                if (atomic_compare_exchange_weak(&entries[index], &old_entry, new_entry)) {
                    if (old_version > 0) {
                        delete_item(table, old_item);
                    }
                    return true;
                }
                // if CAS was failed then try again
            } else {
                // check that entry wasn't changed
                new_entry = atomic_load(&entries[index]);
                if (new_entry == old_entry) {
                    // if entry is same, we've found a collision, so go to the next entry
                    break;
                }
                old_entry = new_entry;
            }
        } while(true);
    }
    return false;
}

bool lockfree_hashtable_find(lockfree_hashtable_t* table, const void* key, void* val)
{
    const lockfree_hashtable_config_t* config = table->config;
    atomic_uint64_t* entries = table->entries;
    atomic_uint64_t* pool = table->pool;

    const uint32_t hash = calc_hash(key, config->key_size);
    for (size_t i = 0, index = hash % config->table_size; i < config->table_size; ++i, index = (index + 1) % config->table_size) {
        // read table entry
        uint64_t entry = atomic_load(&entries[index]);
        do {
            const uint32_t item = entry;
            const uint32_t version = entry >> 32u;

            // version == 0 means free entry
            if (version == 0) {
                return false;
            }
            // if entry is deleted
            if (item == NULL_ITEM) {
                break;
            }
            // compare keys
            if (memcmp(key, get_item_key(table, item), config->key_size) == 0) {
                // copy value if "val" is not NULL
                if (val != NULL) {
                    memcpy(val, get_item_val(table, item), config->val_size);
                }
                // check that entry wasn't changed
                const uint64_t new_entry = atomic_load(&entries[index]);
                if (new_entry == entry) {
                    // if entry is same then return success
                    return true;
                }
                // we've found that somebody changed our entry, try again
                entry = new_entry;
            } else {
                // if keys are not equal, maybe somebody changed our entry, check it
                const uint64_t new_entry = atomic_load(&entries[index]);
                if (new_entry == entry) {
                    // if entry is same, so go to the next entry
                    break;
                }
                entry = new_entry;
            }
        } while(true);
    }
    return false;
}

bool lockfree_hashtable_erase(lockfree_hashtable_t* table, const void* key)
{
    const lockfree_hashtable_config_t* config = table->config;
    atomic_uint64_t* entries = table->entries;
    atomic_uint64_t* pool = table->pool;

    const uint32_t hash = calc_hash(key, config->key_size);

    for (size_t i = 0, index = hash % config->table_size; i < config->table_size; ++i, index = (index + 1) % config->table_size) {
        // read table entry
        uint64_t old_entry = atomic_load(&entries[index]);
        do {
            const uint32_t old_item = old_entry;
            const uint32_t old_version = old_entry >> 32u;

            // mark item as deleted
            uint32_t new_item = NULL_ITEM;
            // increment a version
            uint32_t new_version = old_version + 1;
            uint64_t new_entry = ((uint64_t)new_version << 32u) | (uint64_t)new_item;

            // version == 0 means free entry
            if (old_version == 0) {
                return false;
            }
            // if entry is deleted
            if (old_item == NULL_ITEM) {
                break;
            }

            // compare keys
            if (memcmp(key, get_item_key(table, old_item), config->key_size) == 0) {
                // try to make a CAS
                if (atomic_compare_exchange_weak(&entries[index], &old_entry, new_entry)) {
                    delete_item(table, old_item);
                    return true;
                }
                // if CAS was failed then try again
            } else {
                // check that entry wasn't changed
                new_entry = atomic_load(&entries[index]);
                if (new_entry == old_entry) {
                    // if entry is same, so go to the next entry
                    break;
                }
                // we've found that somebody changed our entry, try again
                old_entry = new_entry;
            }
        } while(true);
    }
    return false;
}
