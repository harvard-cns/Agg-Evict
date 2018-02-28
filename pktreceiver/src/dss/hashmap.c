#include "../common.h"
#include "../vendor/murmur3.h"

#include "rte_atomic.h"
#include "rte_malloc.h"
#include "rte_memcpy.h"

#include "hash.h"
#include "memcmp.h"

#include "hashmap.h"

struct HashMap {
    uint32_t count;
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    dss_cmp_func cmp;

#ifdef _COUNT_MEMORY_STATS_
    rte_atomic32_t stats_search;
#endif

    int table[];
};


inline uint32_t get_size_ca(HashMapPtr ptr)
{
    return ptr->size;
}

inline int* get_table_ca(HashMapPtr ptr)
{
    return ptr->table;
}

inline void 
hashmap_report(HashMapPtr ptr, FILE * minhash_res)
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
hashmap_reset(HashMapPtr ptr) {
    ptr->count = 0;
#ifdef _COUNT_MEMORY_STATS_
    rte_atomic32_set(&ptr->stats_search, 0);
#endif
    memset(ptr->table, 0, ptr->size * (ptr->elsize + ptr->keysize) * sizeof(uint32_t));
}

HashMapPtr
hashmap_create(uint32_t size, uint16_t keysize,
    uint16_t elsize, int socket) {
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    HashMapPtr ptr = rte_zmalloc_socket(0, 
        sizeof(struct HashMap) + /* DS */
        (size) * (elsize + keysize) * sizeof(uint32_t), /* Size of elements */
        64, socket);

    ptr->size = size-1;
    ptr->count = 0;
    ptr->elsize = elsize;
    ptr->keysize = keysize;
    ptr->cmp = dss_cmp(keysize);
    ptr->rowsize = ptr->elsize + ptr->keysize;

    return ptr;
}

inline void *
hashmap_get_copy_key(HashMapPtr ptr, void const *key) {
    uint32_t hash = dss_hash(key, ptr->keysize);
    uint32_t *ret = (((uint32_t*)ptr->table) + /* Base addr */
            (hash & ptr->size) * (ptr->rowsize)); /* Index */
    rte_memcpy(ret, key, ptr->keysize*4);
#ifdef _COUNT_MEMORY_STATS_
    rte_atomic32_inc(&ptr->stats_search);
#endif
    return (ret + ptr->keysize);
}

inline void *
hashmap_get_nocopy_key(HashMapPtr ptr, void const *key) {
    uint32_t hash = dss_hash(key, ptr->keysize);
    uint32_t *ret = (((uint32_t*)ptr->table) + /* Base addr */
            (hash & ptr->size) * (ptr->rowsize)); /* Index */
    ptr->count++;
#ifdef _COUNT_MEMORY_STATS_
    rte_atomic32_inc(&ptr->stats_search);
#endif
    return (ret + ptr->keysize);
}

inline void *
hashmap_get_with_hash(HashMapPtr ptr, uint32_t hash) {
    uint32_t *ret = (((uint32_t*)ptr->table) + /* Base addr */
            (hash & ptr->size) * (ptr->rowsize)); /* Index */
    ptr->count++;
#ifdef _COUNT_MEMORY_STATS_
    rte_atomic32_inc(&ptr->stats_search);
#endif
    return (ret + ptr->keysize);
}

inline void
hashmap_delete(HashMapPtr ptr) {
    rte_free(ptr);
}

inline void *
hashmap_begin(HashMapPtr ptr){
    return (((uint32_t*)ptr->table) + (ptr->keysize));
}

inline void *
hashmap_end(HashMapPtr ptr){
    return (((uint32_t*)ptr->table) + (ptr->rowsize * (ptr->size)) + ptr->keysize);
}

inline void *
hashmap_next(HashMapPtr ptr, void *cur) {
    return ((uint32_t*)cur) + ptr->rowsize;
}

inline uint32_t 
hashmap_size(HashMapPtr ptr) {
    return ptr->size;
}

inline uint32_t
hashmap_count(HashMapPtr ptr) {
    return ptr->count;
}

inline uint32_t
hashmap_num_searches(HashMapPtr ptr){ 
#ifdef _COUNT_MEMORY_STATS_
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
#else
    (void)(ptr);
    return 0;
#endif
}
