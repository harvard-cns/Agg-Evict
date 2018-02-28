#include <assert.h>

#include "../common.h"
#include "../vendor/murmur3.h"

#include "rte_atomic.h"
#include "rte_malloc.h"
#include "rte_memcpy.h"
#include "rte_memory.h"

#include "hash.h"
#include "memcmp.h"
#include "hashmap_cuckoo.h"

#define HASHMAP_CUCKOO_MAX_TRIES 32

struct HashMapCuckoo {
    uint32_t count;
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    dss_cmp_func cmp;
    rte_atomic32_t stats_search;

    uint8_t *table;

    uint8_t *tblpri;
    uint8_t *tblsec;

    uint8_t *scratch_1;
    uint8_t *scratch_2;

    uint8_t end[];
};

/* Algorithm and ideas mostly taken from:
 *  1) http://cs.stanford.edu/~rishig/courses/ref/l13a.pdf
 *  2) librte_hash
 */

inline static uint32_t
hashmap_cuckoo_total_size(HashMapCuckooPtr ptr) {
    return ((ptr->size+1) * 2 * (ptr->rowsize)) * sizeof(uint32_t);
}

inline void
hashmap_cuckoo_reset(HashMapCuckooPtr ptr) {
    ptr->count = 0;
    rte_atomic32_set(&ptr->stats_search, 0);

    memset(ptr->table, 0, hashmap_cuckoo_total_size(ptr));
    memset(ptr->scratch_1, 0, ptr->rowsize);
    memset(ptr->scratch_2, 0, ptr->rowsize);
}

HashMapCuckooPtr
hashmap_cuckoo_create(uint32_t size, uint16_t keysize,
    uint16_t elsize, int socket) {

    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    HashMapCuckooPtr ptr = rte_zmalloc_socket(0, 
        sizeof(struct HashMapCuckoo) + 4*(elsize + keysize + 1)*2,
        64, socket);

    ptr->size    = size-1;
    ptr->count   = 0;
    ptr->elsize  = elsize;
    ptr->keysize = keysize;
    ptr->rowsize = ptr->elsize + ptr->keysize + 1;

    ptr->table = rte_zmalloc_socket(0,
            hashmap_cuckoo_total_size(ptr), 64, socket);
    ptr->cmp = dss_cmp(keysize);

    ptr->tblpri = ptr->table;
    ptr->tblsec = ptr->table + hashmap_cuckoo_total_size(ptr) / 2;

    ptr->scratch_1 = ptr->end;
    ptr->scratch_2 = ptr->end + ptr->rowsize * 4;

    return ptr;
}

/* Taken from rte_hash library */
inline static uint32_t
hash_sec(uint32_t primary_hash) {
    static const unsigned all_bits_shift = 12;
    static const unsigned alt_bits_xor = 0x5bd1e995;

    uint32_t tag = primary_hash >> all_bits_shift;

    return (primary_hash ^ ((tag + 1) * alt_bits_xor));
}

inline static uint32_t hash_offset(
        HashMapCuckooPtr ptr, uint32_t idx) {
    return ((idx & ptr->size) * (ptr->rowsize) * sizeof(uint32_t));
}

inline static void *find_key(
        HashMapCuckooPtr ptr, void const *key) {
    /* Check primary and secondary table */
    uint32_t hash = dss_hash(key, ptr->keysize);
    uint8_t *keypos = &ptr->tblpri[hash_offset(ptr, hash)];
    rte_prefetch0(keypos);

    uint32_t hash_2 = hash_sec(hash);
    uint8_t *keypos_2 = &ptr->tblsec[hash_offset(ptr, hash_2)];
    rte_prefetch0(keypos_2);

    if (ptr->cmp(keypos, key, ptr->keysize) == 0)
        return keypos;

    if (ptr->cmp(keypos_2, key, ptr->keysize) == 0)
        return keypos_2;

    return 0;
}

inline static void print_line(void const *key) {
    uint32_t const *ptr = (uint32_t const*)key;
    printf("%p, %"PRIu64" -> %"PRIu64" (hash: %u)\n",
            ptr, *(uint64_t const*)ptr, *(uint64_t const*)(ptr+3), *(ptr+2));

}

inline static void *find_or_insert_key(
        HashMapCuckooPtr ptr, void const *key) {
    /* Check primary and secondary table */
    uint32_t hash = dss_hash(key, ptr->keysize);
    uint8_t *keypos = &ptr->tblpri[hash_offset(ptr, hash)];
    rte_prefetch0(keypos);

    rte_atomic32_inc(&ptr->stats_search);
    if (ptr->cmp(keypos, key, ptr->keysize) == 0)
        return keypos;

    uint32_t hash_2 = hash_sec(hash);
    uint8_t *keypos_2 = &ptr->tblsec[hash_offset(ptr, hash_2)];
    rte_prefetch0(keypos_2);

    rte_atomic32_inc(&ptr->stats_search);
    if (ptr->cmp(keypos_2, key, ptr->keysize) == 0)
        return keypos_2;

    uint8_t i = 0;
    uint8_t *ret = keypos;

    memset(ptr->scratch_1, 0, ptr->rowsize*4);
    rte_memcpy(ptr->scratch_1, key, ptr->keysize*4);
    rte_memcpy(ptr->scratch_1 + ptr->keysize*4, &hash, sizeof(uint32_t));

    do {
        rte_memcpy(ptr->scratch_2, keypos, ptr->rowsize*4);
        rte_memcpy(keypos, ptr->scratch_1, ptr->rowsize*4);
        if (ptr->cmp(keypos, key, ptr->keysize) == 0) ret = keypos;
        rte_atomic32_inc(&ptr->stats_search);
        if (*((uint32_t*)(ptr->scratch_2)) == 0) {
            return ret;
        }

        /* Update keypos */
        hash = (*((uint32_t*)(ptr->scratch_2 + 4* ptr->keysize)));
        hash = hash_sec(hash);
        keypos = &ptr->tblsec[hash_offset(ptr, hash)];

        rte_memcpy(ptr->scratch_1, keypos, ptr->rowsize*4);
        rte_memcpy(keypos, ptr->scratch_2, ptr->rowsize*4);
        if (ptr->cmp(keypos, key, ptr->keysize) == 0) ret = keypos;
        rte_atomic32_inc(&ptr->stats_search);
        if (*((uint32_t*)(ptr->scratch_1)) == 0) {
            return ret;
        }

        hash = (*((uint32_t*)(ptr->scratch_1 + 4* ptr->keysize)));
        keypos = &ptr->tblpri[hash_offset(ptr, hash)];
        i++;
    }while (i < HASHMAP_CUCKOO_MAX_TRIES);

    assert(0);

    return 0;
}

inline void *
hashmap_cuckoo_get_copy_key(HashMapCuckooPtr ptr, void const *key) {
    void *ret = find_or_insert_key(ptr, key);
    return (((uint32_t*)ret) + ptr->keysize + 1);
}

inline void *
hashmap_cuckoo_get_nocopy_key(HashMapCuckooPtr ptr, void const *key) {
    void *ret = find_or_insert_key(ptr, key);
    return (((uint32_t*)ret) + ptr->keysize + 1);
}

inline void *
hashmap_cuckoo_get_with_hash(HashMapCuckooPtr ptr, uint32_t hash) {
    (void)(ptr);
    (void)(hash);
    assert(0);
    return 0;
}

inline void
hashmap_cuckoo_delete(HashMapCuckooPtr ptr) {
    rte_free(ptr->table);
    rte_free(ptr);
}

inline void *
hashmap_cuckoo_begin(HashMapCuckooPtr ptr){
    assert(0);
    return (((uint32_t*)ptr->table) + (ptr->keysize));
}

inline void *
hashmap_cuckoo_end(HashMapCuckooPtr ptr){
    assert(0);
    return (((uint32_t*)ptr->table) + (ptr->rowsize * (ptr->size)) + ptr->keysize);
}

inline void *
hashmap_cuckoo_next(HashMapCuckooPtr ptr, void *cur) {
    assert(0);
    return ((uint32_t*)cur) + ptr->rowsize;
}

inline uint32_t 
hashmap_cuckoo_size(HashMapCuckooPtr ptr) {
    return ptr->size;
}

inline uint32_t
hashmap_cuckoo_count(HashMapCuckooPtr ptr) {
    return ptr->count;
}

inline uint32_t 
hashmap_cuckoo_num_searches(HashMapCuckooPtr ptr) {
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}
