#include "rte_atomic.h"
#include "rte_memcpy.h"
#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"


#include <stdio.h>
#include <stdlib.h>
#include <bmiintrin.h>


#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "../../vendor/murmur3.h"
#include "../../dss/hash.h"

#include "../heavyhitter/common.h"

#include "fmsketchsimd.h"





inline void fmsketchsimd_reset(FMSketchSimdPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
    
    for(int i = 0; i < SIMD_ROW_NUM; i++)
    {
        uint32_t *p_key_bucket = (ptr->simd_ptr->key + (i << 4));
        rte_prefetch0(p_key_bucket);
        
        for(int j = 0; j < 16; j++)
        {
            fmsketchsimd_inc_fm(ptr, (const void *)(p_key_bucket + j));
        }
    }

    FILE *file_fm = fopen("file_fm_simd.txt", "a");
    fprintf(file_fm ,"%u\n", (uint32_t)ptr->count1);
    fprintf(file_fm ,"%u\n", (uint32_t)ptr->count2);
    fprintf(file_fm ,"%u\n", (uint32_t)ptr->count3);
    fprintf(file_fm ,"%u\n", (uint32_t)ptr->count4);
    fprintf(file_fm, "--------------------------2M-------------------------\n");
    fflush(file_fm);
    fclose(file_fm);



    ptr->count1 = 0;
    ptr->count2 = 0;
    ptr->count3 = 0;
    ptr->count4 = 0;

    // init the simd part;
    memset(ptr->simd_ptr->table, 0, SIMD_SIZE);

#ifdef GRR_ALL
    ptr->simd_ptr->curkick = 0;
#endif

#ifdef LRU_ALL
    ptr->simd_ptr->time_now = 0;
#endif
}

FMSketchSimdPtr fmsketchsimd_create(uint16_t keysize, int socket) 
{
    FMSketchSimdPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct FMSketchSimd), // we do not have any table;
        64, socket);

    ptr->keysize = keysize;

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

inline int fmsketchsimd_inc_fm(FMSketchSimdPtr ptr, void const * kick_key)
{
    uint32_t h1 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH1);
    uint32_t h2 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH2 + CMS_SH1);
    uint32_t h3 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH3 + CMS_SH2);
    uint32_t h4 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH4 + CMS_SH3);

    uint32_t hnum1 = (h1 == 0 ? 31 : _tzcnt_u32(h1));
    uint32_t hnum2 = (h2 == 0 ? 31 : _tzcnt_u32(h2));
    uint32_t hnum3 = (h3 == 0 ? 31 : _tzcnt_u32(h3));
    uint32_t hnum4 = (h4 == 0 ? 31 : _tzcnt_u32(h4));

    ptr->count1 |= (1 << hnum1);
    ptr->count2 |= (1 << hnum2);
    ptr->count3 |= (1 << hnum3);
    ptr->count4 |= (1 << hnum4);
    
    return 1;
}


inline int fmsketchsimd_inc(FMSketchSimdPtr ptr, void const *key) 
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

    uint32_t ret = fmsketchsimd_inc_fm(ptr, (const void *)(p_key_bucket + curkick));

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
    uint32_t ret = fmsketchsimd_inc_fm(ptr, (const void *)(p_key_bucket + kick_index));

    p_key_bucket[kick_index] = flowid;
    p_value_bucket[kick_index] = 1;


    p_clock_bucket[kick_index] = ptr->simd_ptr->time_now;

    return ret;
#endif
}

inline void fmsketchsimd_delete(FMSketchSimdPtr ptr) 
{
    rte_free(ptr->simd_ptr);
    rte_free(ptr);
}

inline uint32_t fmsketchsimd_num_searches(FMSketchSimdPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_fmsketchsimd_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchFMSketchSimdPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchFMSketchSimd), 64, socket); 

    module->_m.execute = simdbatch_fmsketchsimd_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->fmsketchsimd_ptr1 = fmsketchsimd_create(keysize, socket);
    module->fmsketchsimd_ptr2 = fmsketchsimd_create(keysize, socket);

    module->fmsketchsimd = module->fmsketchsimd_ptr1;

    return (ModulePtr)module;
}

void simdbatch_fmsketchsimd_delete(ModulePtr module_) 
{
    ModuleSimdBatchFMSketchSimdPtr module = (ModuleSimdBatchFMSketchSimdPtr)module_;

    fmsketchsimd_delete(module->fmsketchsimd_ptr1);
    fmsketchsimd_delete(module->fmsketchsimd_ptr2);

    rte_free(module);
}

inline void simdbatch_fmsketchsimd_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchFMSketchSimdPtr module = (ModuleSimdBatchFMSketchSimdPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    FMSketchSimdPtr fmsketchsimd = module->fmsketchsimd;

    /* Prefetch fmsketchsimd entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        fmsketchsimd_inc(fmsketchsimd, (pkt + 26));     
    }
}

inline void simdbatch_fmsketchsimd_reset(ModulePtr module_) 
{
    ModuleSimdBatchFMSketchSimdPtr module = (ModuleSimdBatchFMSketchSimdPtr)module_;
    FMSketchSimdPtr prev = module->fmsketchsimd;

    if (module->fmsketchsimd == module->fmsketchsimd_ptr1) 
    {
        module->fmsketchsimd = module->fmsketchsimd_ptr2;
    } 
    else 
    {
        module->fmsketchsimd = module->fmsketchsimd_ptr1;
    }

    module->stats_search += fmsketchsimd_num_searches(prev);
    fmsketchsimd_reset(prev);
}

inline void simdbatch_fmsketchsimd_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchFMSketchSimdPtr module = (ModuleSimdBatchFMSketchSimdPtr)module_;
    module->stats_search += fmsketchsimd_num_searches(module->fmsketchsimd);
    fprintf(f, "SimdBatch::FMSketchSimd::SearchLoad\t%u\n", module->stats_search);
}




