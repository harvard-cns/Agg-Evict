#include "rte_atomic.h"
#include "rte_memcpy.h"
#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"


#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <bmiintrin.h>

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "../../vendor/murmur3.h"
#include "../../dss/hash.h"

#include "../heavyhitter/common.h"

#include "cmsketchsimd.h"



inline static uint32_t cmsketchsimd_size(CMSketchSimdPtr ptr) 
{
    return ptr->size * 4 + 4;
}

inline void cmsketchsimd_reset(CMSketchSimdPtr ptr) 
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
            cmsketchsimd_inc_cm(ptr, (const void *)(p_key_bucket + j), *(p_value_bucket + j));
        }
    }

    memset(ptr->table, 0, cmsketchsimd_size(ptr) * (ptr->elsize) * sizeof(uint32_t));
    

    // init the simd part;
    memset(ptr->simd_ptr->table, 0, SIMD_SIZE);

#ifdef GRR_ALL
    ptr->simd_ptr->curkick = 0;
#endif

#ifdef LRU_ALL
    ptr->simd_ptr->time_now = 0;
#endif
}

CMSketchSimdPtr cmsketchsimd_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    CMSketchSimdPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct CMSketchSimd) + 4 * (size) * (elsize) * sizeof(uint32_t), /* Size of elements */
        64, socket);
    // size -> the number of counters in each row;
    // elsize -> the number of uint32_t in each counter;


    // for (h & ptr->size) to function as %;     
    ptr->size = size - 1;
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



inline static uint32_t cmsketchsimd_offset(CMSketchSimdPtr ptr, uint32_t h) 
{
    return ((h & ptr->size) * ptr->elsize);
}

inline int cmsketchsimd_inc_cm(CMSketchSimdPtr ptr, void const *kick_key, uint32_t kick_value)
{

    uint32_t h1 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH1);
    uint32_t *p1 = ptr->row1 + cmsketchsimd_offset(ptr, h1);
    rte_prefetch0(p1);

    uint32_t h2 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH2 + CMS_SH1);
    uint32_t *p2 = ptr->row2 + cmsketchsimd_offset(ptr, h2);
    rte_prefetch0(p2);

    uint32_t h3 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH3 + CMS_SH2);
    uint32_t *p3 = ptr->row3 + cmsketchsimd_offset(ptr, h3);
    rte_prefetch0(p3);

    uint32_t h4 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH4 + CMS_SH3);
    uint32_t *p4 = ptr->row4 + cmsketchsimd_offset(ptr, h4);
    rte_prefetch0(p4);


    (*p1) += kick_value;
    (*p2) += kick_value;
    (*p3) += kick_value;
    (*p4) += kick_value;

    return 1;
}

inline int cmsketchsimd_inc(CMSketchSimdPtr ptr, void const *key) 
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

    uint32_t ret = cmsketchsimd_inc_cm(ptr, (const void *)(p_key_bucket + curkick), p_value_bucket[curkick]);

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
    uint32_t ret = cmsketchsimd_inc_cm(ptr, (const void *)(p_key_bucket + kick_index), p_value_bucket[kick_index]);

    p_key_bucket[kick_index] = flowid;
    p_value_bucket[kick_index] = 1;


    p_clock_bucket[kick_index] = ptr->simd_ptr->time_now;

    return ret;
#endif
}

inline void cmsketchsimd_delete(CMSketchSimdPtr ptr) 
{
    rte_free(ptr->simd_ptr);
    rte_free(ptr);
}

inline uint32_t cmsketchsimd_num_searches(CMSketchSimdPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_cmsketchsimd_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");

    ModuleSimdBatchCMSketchSimdPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchCMSketchSimd), 64, socket); 

    module->_m.execute = simdbatch_cmsketchsimd_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->cmsketchsimd_ptr1 = cmsketchsimd_create(size, keysize, elsize, socket);
    module->cmsketchsimd_ptr2 = cmsketchsimd_create(size, keysize, elsize, socket);

    module->cmsketchsimd = module->cmsketchsimd_ptr1;

    return (ModulePtr)module;
}

void simdbatch_cmsketchsimd_delete(ModulePtr module_) 
{
    ModuleSimdBatchCMSketchSimdPtr module = (ModuleSimdBatchCMSketchSimdPtr)module_;

    cmsketchsimd_delete(module->cmsketchsimd_ptr1);
    cmsketchsimd_delete(module->cmsketchsimd_ptr2);

    rte_free(module);
}

inline void simdbatch_cmsketchsimd_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchCMSketchSimdPtr module = (ModuleSimdBatchCMSketchSimdPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    CMSketchSimdPtr cmsketchsimd = module->cmsketchsimd;

    /* Prefetch cmsketchsimd entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        cmsketchsimd_inc(cmsketchsimd, (pkt + 26)); 
    }
}

inline void simdbatch_cmsketchsimd_reset(ModulePtr module_) 
{
    ModuleSimdBatchCMSketchSimdPtr module = (ModuleSimdBatchCMSketchSimdPtr)module_;
    CMSketchSimdPtr prev = module->cmsketchsimd;

    if (module->cmsketchsimd == module->cmsketchsimd_ptr1) 
    {
        module->cmsketchsimd = module->cmsketchsimd_ptr2;
    } 
    else 
    {
        module->cmsketchsimd = module->cmsketchsimd_ptr1;
    }

    module->stats_search += cmsketchsimd_num_searches(prev);
    cmsketchsimd_reset(prev);
}

inline void simdbatch_cmsketchsimd_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchCMSketchSimdPtr module = (ModuleSimdBatchCMSketchSimdPtr)module_;
    module->stats_search += cmsketchsimd_num_searches(module->cmsketchsimd);
    fprintf(f, "SimdBatch::CMSketchSimd::SearchLoad\t%u\n", module->stats_search);
}

