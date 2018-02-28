#include "../common.h"
#include "../vendor/murmur3.h"

#include "rte_atomic.h"
#include "rte_malloc.h"
#include "rte_memcpy.h"

#include "hash.h"
#include "memcmp.h"

#include "hashmap_linear.h"

struct HashMapLinear {
    uint32_t count;
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    dss_cmp_func cmp;
    rte_atomic32_t stats_search;

    uint32_t *eot;
    uint32_t table[];
};

inline uint32_t get_size(HashMapLinearPtr ptr)
{
    return ptr->size;
}

inline uint32_t* get_table(HashMapLinearPtr ptr)
{
    return ptr->table;
}

inline void 
hashmap_linear_report(HashMapLinearPtr ptr, FILE * minhash_res)
{
    uint32_t hh_num = 0;
    for(uint32_t i = 0; i < ptr->size; i++)
    {
        uint64_t key = *(uint64_t*)(ptr->table + i * 3);
        if(key != 0)
            hh_num++;
    }
    fprintf(minhash_res, "%u;", hh_num);

    for(uint32_t i = 0; i < ptr->size; i++)
    {
        uint64_t key = *(uint64_t*)(ptr->table + i * 3);
        uint32_t value = *(uint32_t*)(ptr->table + i * 3 + 2);
        if(key != 0)
            fprintf(minhash_res, "%llu,%u;", (long long unsigned int)key, value);
    }
    // fwrite(ptr->table, sizeof(uint32_t), ptr->size * (ptr->elsize + ptr->keysize), minhash_res);
}


inline void
hashmap_linear_reset(HashMapLinearPtr ptr) {
    ptr->count = 0;
    rte_atomic32_set(&ptr->stats_search, 0);
    memset(ptr->table, 0, ptr->size * (ptr->elsize + ptr->keysize) * sizeof(uint32_t));
}

HashMapLinearPtr
hashmap_linear_create(uint32_t size, uint16_t keysize,
    uint16_t elsize, int socket) {
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    HashMapLinearPtr ptr = rte_zmalloc_socket(0, 
        sizeof(struct HashMapLinear) + /* DS */
        (size) * (elsize + keysize) * sizeof(uint32_t), /* Size of elements */
        64, socket);

    ptr->size = size-1;
    ptr->count = 0;
    ptr->elsize = elsize;
    ptr->keysize = keysize;
    ptr->rowsize = ptr->elsize + ptr->keysize;
    ptr->eot = ptr->table + (ptr->rowsize * ptr->size);
    ptr->cmp = dss_cmp(keysize);

    return ptr;
}

inline static void *
find_key(HashMapLinearPtr ptr, void const *key, void *ret) {
    uint32_t *init = (uint32_t*)ret;
    rte_atomic32_inc(&ptr->stats_search);
    while (1) {
        if (*init == 0) return init;
        if (ptr->cmp(init, key, ptr->keysize) == 0) return init;
        init = init + ptr->rowsize;
        rte_atomic32_inc(&ptr->stats_search);
        if (init > ptr->eot) init = (uint32_t *)ptr->table;
    }
}

inline static void *
find_and_copy_key(HashMapLinearPtr ptr, void const *key, void *ret) {
    uint32_t *init = (uint32_t*)ret;
    rte_atomic32_inc(&ptr->stats_search);
    while (1) {
        if (*init == 0) { rte_memcpy(init, key, ptr->keysize * 4); return init; };
        if (ptr->cmp(init, key, ptr->keysize) == 0) return init;
        init = init + ptr->rowsize;
        rte_atomic32_inc(&ptr->stats_search);
        if (init > ptr->eot) init = (uint32_t *)ptr->table;
    }
}

inline void *
hashmap_linear_get_copy_key(HashMapLinearPtr ptr, void const *key) {
    uint32_t hash = dss_hash(key, ptr->keysize);
    uint32_t *ret = (((uint32_t*)ptr->table) + /* Base addr */
            (hash & ptr->size) * (ptr->rowsize)); /* Index */
    ptr->count++;
    ret = find_and_copy_key(ptr, key, ret);
    return (ret + ptr->keysize);

}

inline void *
hashmap_linear_get_nocopy_key(HashMapLinearPtr ptr, void const *key) {
    uint32_t hash = dss_hash(key, ptr->keysize);
    uint32_t *ret = (((uint32_t*)ptr->table) + /* Base addr */
            (hash & ptr->size) * (ptr->rowsize)); /* Index */
    ptr->count++;
    ret = find_key(ptr, key, ret);
    return (ret + ptr->keysize);
}

inline void *
hashmap_linear_get_with_hash(HashMapLinearPtr ptr, uint32_t hash) {
    uint32_t *ret = (((uint32_t*)ptr->table) + /* Base addr */
            (hash & ptr->size) * (ptr->rowsize)); /* Index */
    ptr->count++;
    ret = find_key(ptr, 0, ret);
    return (ret + ptr->keysize);
}

inline void
hashmap_linear_delete(HashMapLinearPtr ptr) {
    rte_free(ptr);
}

inline void *
hashmap_linear_begin(HashMapLinearPtr ptr){
    return (((uint32_t*)ptr->table) + (ptr->keysize));
}

inline void *
hashmap_linear_end(HashMapLinearPtr ptr){
    return (((uint32_t*)ptr->table) + (ptr->rowsize * (ptr->size)) + ptr->keysize);
}

inline void *
hashmap_linear_next(HashMapLinearPtr ptr, void *cur) {
    return ((uint32_t*)cur) + ptr->rowsize;
}

inline uint32_t 
hashmap_linear_size(HashMapLinearPtr ptr) {
    return ptr->size;
}

inline uint32_t
hashmap_linear_count(HashMapLinearPtr ptr) {
    return ptr->count;
}

inline uint32_t
hashmap_linear_num_searches(HashMapLinearPtr ptr) {
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}
