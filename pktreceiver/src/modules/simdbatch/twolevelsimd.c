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

#include "twolevelsimd.h"


inline void twolevelsimd_reset(TwoLevelSimdPtr ptr) 
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
            twolevelsimd_inc_tl(ptr, (const void *)(p_key_bucket + j));
        }
    }

    memset(ptr->table, 0, 4 * ptr->size * sizeof(struct BitCell) + 4 * 4096 * sizeof(struct BitCell));
    

    // init the simd part;
    memset(ptr->simd_ptr->table, 0, SIMD_SIZE);
    
#ifdef GRR_ALL
    ptr->simd_ptr->curkick = 0;
#endif

#ifdef LRU_ALL
    ptr->simd_ptr->time_now = 0;
#endif
}

TwoLevelSimdPtr twolevelsimd_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    TwoLevelSimdPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct TwoLevelSimd) + 
        4 * size * sizeof(struct BitCell) + 4 * 4096 * sizeof(struct BitCell), /* Size of elements */
        64, socket);


    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->rowrev1 = (BitCellPtr)ptr->table;
    ptr->rowrev2 = (BitCellPtr)(ptr->table + 4096 * 1 * 3);
    ptr->rowrev3 = (BitCellPtr)(ptr->table + 4096 * 2 * 3);
    ptr->rowrev4 = (BitCellPtr)(ptr->table + 4096 * 3 * 3);

    ptr->rowcm1 = (BitCellPtr)(ptr->table + 4096 * 4 * 3);
    ptr->rowcm2 = (BitCellPtr)(ptr->table + 4096 * 4 * 3 + size * 1 * 3);
    ptr->rowcm3 = (BitCellPtr)(ptr->table + 4096 * 4 * 3 + size * 2 * 3);
    ptr->rowcm4 = (BitCellPtr)(ptr->table + 4096 * 4 * 3 + size * 3 * 3);


    // allocate memory for simd part;
    ptr->simd_ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct Simd) + SIMDSD_SIZE, /* Size of elements */
        64, socket);

    ptr->simd_ptr->key = ptr->simd_ptr->table;
    ptr->simd_ptr->value = ptr->simd_ptr->table + (KEY_SIZE);
    ptr->simd_ptr->sdpair = (uint64_t *)(ptr->simd_ptr->table + (KEY_SIZE * 2));


#ifdef GRR_ALL
    ptr->simd_ptr->curpos = ptr->simd_ptr->table + (KEY_SIZE * 4);
#endif

#ifdef LRU_ALL
    ptr->simd_ptr->myclock = ptr->simd_ptr->table + (KEY_SIZE * 4);
    ptr->simd_ptr->curpos = ptr->simd_ptr->table + (KEY_SIZE * 5);
#endif

    return ptr;
}



inline void cell_inc_simd(BitCellPtr ptr, void const * key)
{
    uint32_t h1 = dss_hash_with_seed(key, 1, CMS_SH5) % 96;
    uint32_t a = h1 >> 5;
    uint32_t b = h1 & 0x1F;

    ptr->cell[a] |= (1 << b);
}

inline int twolevelsimd_inc_tl(TwoLevelSimdPtr ptr, void const * kick_key)
{
    uint32_t h1 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH1) & 0x7;
    uint32_t h2 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH2 + CMS_SH1) & 0x7;
    uint32_t h3 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH3 + CMS_SH2) & 0x7;
    uint32_t h4 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH4 + CMS_SH3) & 0x7;

    BitCellPtr p1 = ptr->rowrev1 + (h1 | (h2 << 3) | (h3 << 6) | (h4 << 9));
    rte_prefetch0(p1);


    h1 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH5) & 0x7;
    h2 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH6 + CMS_SH5) & 0x7;
    h3 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH7 + CMS_SH6) & 0x7;
    h4 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH8 + CMS_SH7) & 0x7;

    BitCellPtr p2 = ptr->rowrev2 + (h1 | (h2 << 3) | (h3 << 6) | (h4 << 9));
    rte_prefetch0(p2);


    h1 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH9) & 0x7;
    h2 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH10 + CMS_SH9) & 0x7;
    h3 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH11 + CMS_SH10) & 0x7;
    h4 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH12 + CMS_SH11) & 0x7;

    BitCellPtr p3 = ptr->rowrev3 + (h1 | (h2 << 3) | (h3 << 6) | (h4 << 9));
    rte_prefetch0(p3);


    h1 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH13) & 0x7;
    h2 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH14 + CMS_SH13) & 0x7;
    h3 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH15 + CMS_SH14) & 0x7;
    h4 = dss_hash_with_seed_rev(kick_key, ptr->keysize, CMS_SH16 + CMS_SH15) & 0x7;

    BitCellPtr p4 = ptr->rowrev4 + (h1 | (h2 << 3) | (h3 << 6) | (h4 << 9));
    rte_prefetch0(p4);

    cell_inc_simd(p1, ((uint32_t const*)kick_key) + 1);
    cell_inc_simd(p2, ((uint32_t const*)kick_key) + 1);
    cell_inc_simd(p3, ((uint32_t const*)kick_key) + 1);
    cell_inc_simd(p4, ((uint32_t const*)kick_key) + 1);



    h1 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH1) & (ptr->size - 1);
    p1 = ptr->rowcm1 + h1;
    rte_prefetch0(p1);

    h2 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH2 + CMS_SH1) & (ptr->size - 1);
    p2 = ptr->rowcm2 + h2;
    rte_prefetch0(p2);

    h3 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH3 + CMS_SH2) & (ptr->size - 1);
    p3 = ptr->rowcm3 + h3;
    rte_prefetch0(p3);

    h4 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH4 + CMS_SH3) & (ptr->size - 1);
    p4 = ptr->rowcm4 + h3;
    rte_prefetch0(p4);

    cell_inc_simd(p1, ((uint32_t const*)kick_key) + 1);
    cell_inc_simd(p2, ((uint32_t const*)kick_key) + 1);
    cell_inc_simd(p3, ((uint32_t const*)kick_key) + 1);
    cell_inc_simd(p4, ((uint32_t const*)kick_key) + 1);

    return 1;

}

inline int twolevelsimd_inc(TwoLevelSimdPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

#ifdef LRU_ALL
    ptr->simd_ptr->time_now ++;
#endif


    uint32_t fp = (*(uint32_t const*)key) ^ (*(((uint32_t const*)key) + 1));
    uint64_t flowid = (*(uint64_t const*)key);

    uint32_t bucket_id = fp % SIMD_ROW_NUM;

    uint32_t *p_key_bucket = (ptr->simd_ptr->key + (bucket_id << 4));
    rte_prefetch0(p_key_bucket);

    uint32_t *p_value_bucket = (ptr->simd_ptr->value + (bucket_id << 4));
    rte_prefetch0(p_value_bucket);

    uint64_t *p_sdpair_bucket = (ptr->simd_ptr->sdpair + (bucket_id << 4));
    rte_prefetch0(p_sdpair_bucket);

#ifdef LRU_ALL
    uint32_t *p_clock_bucket = (ptr->simd_ptr->myclock + (bucket_id << 4));
    rte_prefetch0(p_clock_bucket);
#endif

    const __m128i item = _mm_set1_epi32((int)fp);

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

        if((*(p_sdpair_bucket + matched_index)) == flowid)
        {
            (*(p_value_bucket + matched_index)) ++;
        }
        else
        {
            twolevelsimd_inc_tl(ptr, (const void *)(p_sdpair_bucket + matched_index));
            (*(p_sdpair_bucket + matched_index)) = flowid;
            (*(p_value_bucket + matched_index)) = 1;
        }

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
        (*(p_sdpair_bucket + cur_pos_now)) = flowid;
        (*(p_key_bucket + cur_pos_now)) = fp;
        (*(p_value_bucket + cur_pos_now)) = 1;

#ifdef LRU_ALL
        p_clock_bucket[cur_pos_now] = ptr->simd_ptr->time_now;
#endif  

        (*(p_curpos)) ++;
        return 0;
    }

#ifdef GRR_ALL

    uint32_t curkick = ptr->simd_ptr->curkick;

    uint32_t ret = twolevelsimd_inc_tl(ptr, (const void *)(p_sdpair_bucket + curkick));

    p_key_bucket[curkick] = fp;
    p_value_bucket[curkick] = 1;
    p_sdpair_bucket[curkick] = flowid;


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
    uint32_t ret = twolevelsimd_inc_tl(ptr, (const void *)(p_sdpair_bucket + kick_index));

    p_key_bucket[kick_index] = fp;
    p_value_bucket[kick_index] = 1;
    p_sdpair_bucket[kick_index] = flowid;


    p_clock_bucket[kick_index] = ptr->simd_ptr->time_now;

    return ret;
#endif
}

inline void twolevelsimd_delete(TwoLevelSimdPtr ptr) 
{
    rte_free(ptr->simd_ptr); 
    rte_free(ptr);
}

inline uint32_t twolevelsimd_num_searches(TwoLevelSimdPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_twolevelsimd_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchTwoLevelSimdPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchTwoLevelSimd), 64, socket); 

    module->_m.execute = simdbatch_twolevelsimd_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->twolevelsimd_ptr1 = twolevelsimd_create(size, keysize, elsize, socket);
    module->twolevelsimd_ptr2 = twolevelsimd_create(size, keysize, elsize, socket);

    module->twolevelsimd = module->twolevelsimd_ptr1;

    return (ModulePtr)module;
}

void simdbatch_twolevelsimd_delete(ModulePtr module_) 
{
    ModuleSimdBatchTwoLevelSimdPtr module = (ModuleSimdBatchTwoLevelSimdPtr)module_;

    twolevelsimd_delete(module->twolevelsimd_ptr1);
    twolevelsimd_delete(module->twolevelsimd_ptr2);

    rte_free(module);
}

inline void simdbatch_twolevelsimd_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchTwoLevelSimdPtr module = (ModuleSimdBatchTwoLevelSimdPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    TwoLevelSimdPtr twolevelsimd = module->twolevelsimd;

    /* Prefetch twolevelsimd entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        twolevelsimd_inc(twolevelsimd, (pkt + 26));
    }
}

inline void simdbatch_twolevelsimd_reset(ModulePtr module_) 
{
    ModuleSimdBatchTwoLevelSimdPtr module = (ModuleSimdBatchTwoLevelSimdPtr)module_;
    TwoLevelSimdPtr prev = module->twolevelsimd;

    if (module->twolevelsimd == module->twolevelsimd_ptr1) 
    {
        module->twolevelsimd = module->twolevelsimd_ptr2;
    } 
    else 
    {
        module->twolevelsimd = module->twolevelsimd_ptr1;
    }

    module->stats_search += twolevelsimd_num_searches(prev);
    twolevelsimd_reset(prev);
}

inline void simdbatch_twolevelsimd_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchTwoLevelSimdPtr module = (ModuleSimdBatchTwoLevelSimdPtr)module_;
    module->stats_search += twolevelsimd_num_searches(module->twolevelsimd);
    fprintf(f, "SimdBatch::TwoLevelSimd::SearchLoad\t%u\n", module->stats_search);
}


