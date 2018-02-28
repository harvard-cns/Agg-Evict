#include "rte_atomic.h"
#include "rte_memcpy.h"
#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"


#include <stdio.h>
#include <stdlib.h>


#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "../../vendor/murmur3.h"
#include "../../dss/hash.h"

#include "../heavyhitter/common.h"

#include "revsketchsimd.h"


inline void revsketchsimd_reset(RevSketchSimdPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
    
    for(int i = 0; i < SIMD_ROW_NUM; i++)
    {
        uint32_t *p_key_bucket = (ptr->simd_ptr->key + (i << 4));
        rte_prefetch0(p_key_bucket);

        uint32_t *p_value_bucket = (ptr->simd_ptr->value + (i << 4));
        rte_prefetch0(p_value_bucket);
        
        for(int j = 0; j < 16; j++)
        {
            revsketchsimd_inc_rs(ptr, (const void *)(p_key_bucket + j), *(p_value_bucket + j));
        }
    }

    memset(ptr->table, 0, 4 * ptr->size * sizeof(uint32_t));


    // init the simd part;
    memset(ptr->simd_ptr->table, 0, SIMD_SIZE);

#ifdef GRR_ALL
    ptr->simd_ptr->curkick = 0;
#endif

#ifdef LRU_ALL
    ptr->simd_ptr->time_now = 0;
#endif
}

RevSketchSimdPtr revsketchsimd_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    RevSketchSimdPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct RevSketchSimd) + 
        4 * size * sizeof(uint32_t), /* Size of elements */
        64, socket);


    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->row1 = ptr->table;
    ptr->row2 = ptr->table + (size * elsize);
    ptr->row3 = ptr->table + (size * 2 * elsize);
    ptr->row4 = ptr->table + (size * 3 * elsize);

    // allocate memory for simd part;
    ptr->simd_ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct Simd) + SIMD_SIZE, /* Size of elements */
        64, socket);

    ptr->simd_ptr->key = ptr->simd_ptr->table;
    ptr->simd_ptr->value = ptr->simd_ptr->table + (KEY_SIZE);


#ifdef GRR_ALL
    ptr->simd_ptr->curpos = ptr->simd_ptr->table + (KEY_SIZE * 2);
#endif

#ifdef LRU_ALL
    ptr->simd_ptr->myclock = ptr->simd_ptr->table + (KEY_SIZE * 2);
    ptr->simd_ptr->curpos = ptr->simd_ptr->table + (KEY_SIZE * 3);
#endif

    return ptr;
}

inline int revsketchsimd_inc_rs(RevSketchSimdPtr ptr, void const * kick_key, uint32_t kick_val)
{
    uint32_t h1 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH1) & 0xF;
    uint32_t h2 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH2 + CMS_SH1) & 0xF;
    uint32_t h3 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH3 + CMS_SH2) & 0xF;
    uint32_t h4 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH4 + CMS_SH3) & 0xF;

    uint32_t *p1 = ptr->row1 + (h1 | (h2 << 4) | (h3 << 8) | (h4 << 12));
    rte_prefetch0(p1);


    h1 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH5) & 0xF;
    h2 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH6 + CMS_SH5) & 0xF;
    h3 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH7 + CMS_SH6) & 0xF;
    h4 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH8 + CMS_SH7) & 0xF;

    uint32_t *p2 = ptr->row2 + (h1 | (h2 << 4) | (h3 << 8) | (h4 << 12));
    rte_prefetch0(p2);


    h1 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH9) & 0xF;
    h2 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH10 + CMS_SH9) & 0xF;
    h3 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH11 + CMS_SH10) & 0xF;
    h4 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH12 + CMS_SH11) & 0xF;

    uint32_t *p3 = ptr->row3 + (h1 | (h2 << 4) | (h3 << 8) | (h4 << 12));
    rte_prefetch0(p3);


    h1 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH13) & 0xF;
    h2 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH14 + CMS_SH13) & 0xF;
    h3 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH15 + CMS_SH14) & 0xF;
    h4 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH16 + CMS_SH15) & 0xF;

    uint32_t *p4 = ptr->row4 + (h1 | (h2 << 4) | (h3 << 8) | (h4 << 12));
    rte_prefetch0(p4);


    (*p1) += kick_val;
    (*p2) += kick_val;
    (*p3) += kick_val;
    (*p4) += kick_val;

    return 1;
}


inline int revsketchsimd_inc(RevSketchSimdPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

#ifdef LRU_ALL
    ptr->simd_ptr->time_now ++;
#endif


    uint32_t flowid = *(uint32_t const*)key;
    uint32_t bucket_id = flowid % SIMD_ROW_NUM;

    uint32_t *p_key_bucket = (ptr->simd_ptr->key + (bucket_id << 4));
    rte_prefetch0(p_key_bucket);

    uint32_t *p_value_bucket = (ptr->simd_ptr->value + (bucket_id << 4));
    rte_prefetch0(p_value_bucket);

#ifdef LRU_ALL
    uint32_t *p_clock_bucket = (ptr->simd_ptr->myclock + (bucket_id << 4));
    rte_prefetch0(p_clock_bucket);
#endif

    const __m128i item = _mm_set1_epi32((int)flowid);

    __m128i *keys_p = (__m128i *)p_key_bucket;


    __m128i a_comp = _mm_cmpeq_epi32(item, keys_p[0]);
    __m128i b_comp = _mm_cmpeq_epi32(item, keys_p[1]);
    __m128i c_comp = _mm_cmpeq_epi32(item, keys_p[2]);
    __m128i d_comp = _mm_cmpeq_epi32(item, keys_p[3]);

    a_comp = _mm_packs_epi32(a_comp, b_comp);
    c_comp = _mm_packs_epi32(c_comp, d_comp);
    a_comp = _mm_packs_epi32(a_comp, c_comp);

    int matched = _mm_movemask_epi8(a_comp);

    if(matched != 0)
    {
        //return 32 if input is zero;
        int matched_index = _tzcnt_u32((uint32_t)matched);
        (*(p_value_bucket + matched_index)) ++;  

#ifdef LRU_ALL
        p_clock_bucket[matched_index] = ptr->simd_ptr->time_now;
#endif

        return 0;
    }

    uint32_t *p_curpos = (ptr->simd_ptr->curpos + bucket_id);
    rte_prefetch0(p_curpos);
    int cur_pos_now = *(p_curpos);


    if(cur_pos_now != 16)
    {
        (*(p_key_bucket + cur_pos_now)) = flowid;
        (*(p_value_bucket + cur_pos_now)) = 1;

#ifdef LRU_ALL
        p_clock_bucket[cur_pos_now] = ptr->simd_ptr->time_now;
#endif  

        (*(p_curpos)) ++;
        return 0;
    }

#ifdef GRR_ALL

    uint32_t curkick = ptr->simd_ptr->curkick;

    uint32_t ret = revsketchsimd_inc_rs(ptr, (const void *)(p_key_bucket + curkick), p_value_bucket[curkick]);

    p_key_bucket[curkick] = flowid;
    p_value_bucket[curkick] = 1;


    ptr->simd_ptr->curkick = (curkick + 1) & 0xF;

    return ret;
#endif 
#ifdef LRU_ALL
    uint32_t kick_index = 0;
    uint32_t min_time = (1 << 30);
    for(int i = 0; i < 16; i++)
    {
        if(p_clock_bucket[i] < min_time)
        {
            min_time = p_clock_bucket[i];
            kick_index = i;
        }
    }
    uint32_t ret = revsketchsimd_inc_rs(ptr, (const void *)(p_key_bucket + kick_index), p_value_bucket[kick_index]);

    p_key_bucket[kick_index] = flowid;
    p_value_bucket[kick_index] = 1;


    p_clock_bucket[kick_index] = ptr->simd_ptr->time_now;

    return ret;
#endif
}

inline void revsketchsimd_delete(RevSketchSimdPtr ptr) 
{
    rte_free(ptr->simd_ptr);
    rte_free(ptr);
}

inline uint32_t revsketchsimd_num_searches(RevSketchSimdPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_revsketchsimd_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchRevSketchSimdPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchRevSketchSimd), 64, socket); 

    module->_m.execute = simdbatch_revsketchsimd_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->revsketchsimd_ptr1 = revsketchsimd_create(size, keysize, elsize, socket);
    module->revsketchsimd_ptr2 = revsketchsimd_create(size, keysize, elsize, socket);

    module->revsketchsimd = module->revsketchsimd_ptr1;

    return (ModulePtr)module;
}

void simdbatch_revsketchsimd_delete(ModulePtr module_) 
{
    ModuleSimdBatchRevSketchSimdPtr module = (ModuleSimdBatchRevSketchSimdPtr)module_;

    revsketchsimd_delete(module->revsketchsimd_ptr1);
    revsketchsimd_delete(module->revsketchsimd_ptr2);

    rte_free(module);
}

inline void simdbatch_revsketchsimd_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchRevSketchSimdPtr module = (ModuleSimdBatchRevSketchSimdPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    RevSketchSimdPtr revsketchsimd = module->revsketchsimd;

    /* Prefetch revsketchsimd entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        revsketchsimd_inc(revsketchsimd, (pkt + 26));
    }
}

inline void simdbatch_revsketchsimd_reset(ModulePtr module_) 
{
    ModuleSimdBatchRevSketchSimdPtr module = (ModuleSimdBatchRevSketchSimdPtr)module_;
    RevSketchSimdPtr prev = module->revsketchsimd;

    if (module->revsketchsimd == module->revsketchsimd_ptr1) 
    {
        module->revsketchsimd = module->revsketchsimd_ptr2;
    } 
    else 
    {
        module->revsketchsimd = module->revsketchsimd_ptr1;
    }

    module->stats_search += revsketchsimd_num_searches(prev);
    revsketchsimd_reset(prev);
}

inline void simdbatch_revsketchsimd_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchRevSketchSimdPtr module = (ModuleSimdBatchRevSketchSimdPtr)module_;
    module->stats_search += revsketchsimd_num_searches(module->revsketchsimd);
    fprintf(f, "SimdBatch::RevSketchSimd::SearchLoad\t%u\n", module->stats_search);
}


