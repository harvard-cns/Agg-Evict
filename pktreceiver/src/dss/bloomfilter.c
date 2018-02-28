#include "rte_prefetch.h"

#include "string.h"

#include "hash.h"
#include "bloomfilter.h"


int bloomfilter_add_key(BFPropPtr bfp, void *bf, void const *key) {
    rte_prefetch0(bf);

    uint32_t *bfa = (uint32_t*)bf;

    uint32_t hash = dss_hash(key, bfp->keylen);
    uint16_t h1 = (hash      ) & BF_MASK;
    uint16_t h2 = (hash >> 10) & BF_MASK;
    uint16_t h3 = (hash >> 20) & BF_MASK;

    uint16_t idx1 = h1 >> BF_BUCKET_SHIFT;
    uint16_t idx2 = h2 >> BF_BUCKET_SHIFT;
    uint16_t idx3 = h3 >> BF_BUCKET_SHIFT;
    
    uint32_t *bf1 = bfa + idx1;
    uint32_t *bf2 = bfa + idx2;
    uint32_t *bf3 = bfa + idx3;

    uint32_t off1 = (1<<(h1 & BF_BUCKET_MASK));
    uint32_t off2 = (1<<(h2 & BF_BUCKET_MASK));
    uint32_t off3 = (1<<(h3 & BF_BUCKET_MASK));

    int is_member = (*bf1 & off1) && (*bf2 & off2) && (*bf3 & off3);

    *bf1 = *bf1 | off1;
    *bf2 = *bf2 | off2;
    *bf3 = *bf3 | off3;

    return is_member;
}

void bloomfilter_reset(BFPropPtr bfp __attribute__((unused)), void *bf) {
    rte_prefetch0(bf);
    memset(bf, 0, 1024 / sizeof(uint8_t));
}

int bloomfilter_is_member(BFPropPtr bfp, void *bf, void const *key) {
    rte_prefetch0(bf);

    uint32_t *bfa = (uint32_t*)bf;

    uint32_t hash = dss_hash(key, bfp->keylen);
    uint16_t h1 = (hash      ) & BF_MASK;
    uint16_t h2 = (hash >> 10) & BF_MASK;
    uint16_t h3 = (hash >> 20) & BF_MASK;

    uint16_t idx1 = h1 >> BF_BUCKET_SHIFT;
    uint16_t idx2 = h2 >> BF_BUCKET_SHIFT;
    uint16_t idx3 = h3 >> BF_BUCKET_SHIFT;
    
    uint32_t *bf1 = bfa + idx1;
    uint32_t *bf2 = bfa + idx2;
    uint32_t *bf3 = bfa + idx3;

    uint32_t off1 = (1<<(h1 & BF_BUCKET_MASK));
    uint32_t off2 = (1<<(h2 & BF_BUCKET_MASK));
    uint32_t off3 = (1<<(h3 & BF_BUCKET_MASK));

    return (*bf1 & off1) && (*bf2 & off2) && (*bf3 & off3);
}
