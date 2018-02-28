#include "../common.h"
#include "../vendor/murmur3.h"

#include "rte_atomic.h"
#include "rte_malloc.h"
#include "rte_memcpy.h"

#include "hash.h"

#include "countmin.h"

struct CountMin {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;

    uint32_t *row1;
    uint32_t *row2;
    uint32_t *row3;

    uint32_t table[];
};

inline static uint32_t
countmin_size(CountMinPtr ptr) {
    return ptr->size * 3;
}

inline void
countmin_reset(CountMinPtr ptr) {
    rte_atomic32_set(&ptr->stats_search, 0);
    memset(ptr->table, 0, countmin_size(ptr) * (ptr->elsize) * sizeof(uint32_t));
}

CountMinPtr
countmin_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) {
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    CountMinPtr ptr = rte_zmalloc_socket(0, 
        sizeof(struct CountMin) + /* DS */
        3 * (size) * (elsize) * sizeof(uint32_t), /* Size of elements */
        64, socket);

    printf("memory of countmin = %lf\n", 3 * (size) * (elsize) * sizeof(uint32_t) * 1.0 / 1024 / 1024);

    ptr->size = size-1;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->row1 = ptr->table;
    ptr->row2 = ptr->table + (size * elsize);
    ptr->row3 = ptr->table + (size * 2 * elsize);

    return ptr;
}

#define CMS_SH1 83233
#define CMS_SH2 52757
#define CMS_SH3 17093

inline static uint32_t
countmin_offset(CountMinPtr ptr, uint32_t h) {
    return ((h & ptr->size) * ptr->elsize);
}

inline int
countmin_inc(CountMinPtr ptr, void const *key, uint32_t threshold) {
    rte_atomic32_inc(&ptr->stats_search);


    // uint16_t *src = (uint16_t*)key;
    // uint16_t *dst = (uint16_t*)key + 4;

    // printf("src = %u\t", *(uint32_t *)(src));
    // printf("dst = %u\n", *(uint32_t *)(dst));


    uint32_t h1 = dss_hash_with_seed(key, ptr->keysize, CMS_SH1);
    uint32_t *p1 = ptr->row1 + countmin_offset(ptr, h1);
    rte_prefetch0(p1);

    uint32_t h2 = dss_hash_with_seed(key, ptr->keysize, CMS_SH2 + CMS_SH1);
    uint32_t *p2 = ptr->row2 + countmin_offset(ptr, h2);
    rte_prefetch0(p2);

    uint32_t h3 = dss_hash_with_seed(key, ptr->keysize, CMS_SH3 + CMS_SH2);
    uint32_t *p3 = ptr->row3 + countmin_offset(ptr, h3);
    rte_prefetch0(p3);


    (*p1)++;
    int all_grtr_thr = (*p1 >= threshold);
    int one_equal_thr = (*p1 == threshold);

    (*p2)++;
    all_grtr_thr = (all_grtr_thr && (*p2 >= threshold));
    one_equal_thr = (one_equal_thr || (*p2 == threshold));

    (*p3)++;
    all_grtr_thr = (all_grtr_thr && (*p3 >= threshold));
    one_equal_thr = (one_equal_thr || (*p3 == threshold));

    return all_grtr_thr && one_equal_thr;
}

inline void
countmin_delete(CountMinPtr ptr) {
    rte_free(ptr);
}

inline uint32_t
countmin_num_searches(CountMinPtr ptr){ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}
