#ifndef _DSS_HASH_H_
#define _DSS_HASH_H_

#include "../vendor/murmur3.h"


inline static uint32_t
dss_hash_with_seed_rev(void const* key, uint16_t keysize, uint32_t seed) {
    uint32_t hash;
    MurmurHash3_x86_32(key, keysize, seed, &hash);
    return hash;
}

inline static uint32_t
dss_hash_with_seed(void const* key, uint16_t keysize, uint32_t seed) {
    uint32_t hash;
    MurmurHash3_x86_32(key, keysize * 4, seed, &hash);
    return hash;
}

inline static uint32_t
dss_hash(void const* key, uint16_t keysize) {
    return dss_hash_with_seed(key, keysize, 1);
}


/* Taken from rte_hash library */
inline static uint32_t
dss_hash_2(uint32_t primary_hash) {
    static const unsigned all_bits_shift = 13;
    static const unsigned alt_bits_xor = 0x5bd1e995;

    uint32_t tag = primary_hash >> all_bits_shift;

    return (primary_hash ^ ((tag + 1) * alt_bits_xor));
}

/* Simliar to rte_hash, constant from murmurhash 1 */
inline static uint32_t
dss_hash_3(uint32_t primary_hash) {
    static const unsigned all_bits_shift = 10;
    static const unsigned alt_bits_xor = 0xc6a4a793;

    uint32_t tag = primary_hash >> all_bits_shift;

    return (primary_hash ^ ((tag + 1) * alt_bits_xor));
}

static unsigned long x=123456789, y=362436069, z=521288629;
inline static unsigned xorshf96(void) {          //period 2^96-1
    unsigned t;
    x ^= x << 16;
    x ^= x >> 5;
    x ^= x << 1;

    t = x;
    x = y;
    y = z;
    z = t ^ x ^ y;

    return z;
}


#endif
