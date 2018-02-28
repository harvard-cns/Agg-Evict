#include "../common.h"
#include "../vendor/murmur3.h"

#include "rte_atomic.h"
#include "rte_malloc.h"
#include "rte_memcpy.h"

#include "hash.h"

#include "countarray.h"

struct CountArray {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;

    uint32_t *row1;

    uint32_t table[];
};

inline static uint32_t
countarray_size(CountArrayPtr ptr) {
    return ptr->size;
}

inline void
countarray_reset(CountArrayPtr ptr) {
    rte_atomic32_set(&ptr->stats_search, 0);
    memset(ptr->table, 0, countarray_size(ptr) * (ptr->elsize) * sizeof(uint32_t));
}

CountArrayPtr
countarray_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) {
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    CountArrayPtr ptr = rte_zmalloc_socket(0, 
        sizeof(struct CountArray) + /* DS */
        (size) * (elsize) * sizeof(uint32_t), /* Size of elements */
        64, socket);

    printf("memory of countarray = %lf\n", (size) * (elsize) * sizeof(uint32_t) * 1.0 / 1024 / 1024);

    ptr->size = size-1;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->row1 = ptr->table;

    return ptr;
}

#define CMS_SH1 83233
#define CMS_SH2 52757
#define CMS_SH3 17093

inline static uint32_t
countarray_offset(CountArrayPtr ptr, uint32_t h) {
    return ((h & ptr->size) * ptr->elsize);
}

inline int
countarray_inc(CountArrayPtr ptr, void const *key, uint32_t threshold) {
    rte_atomic32_inc(&ptr->stats_search);


    uint32_t h1 = dss_hash_with_seed(key, ptr->keysize, CMS_SH1);
    uint32_t *p1 = ptr->row1 + countarray_offset(ptr, h1);
    rte_prefetch0(p1);


    (*p1)++;
    int all_grtr_thr = (*p1 >= threshold);
    int one_equal_thr = (*p1 == threshold);

    return all_grtr_thr && one_equal_thr;
}

inline void
countarray_delete(CountArrayPtr ptr) {
    rte_free(ptr);
}

inline uint32_t
countarray_num_searches(CountArrayPtr ptr){ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}
