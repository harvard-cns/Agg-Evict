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

#include "flowradarnosimd.h"




inline static uint32_t flowradarnosimd_size(FlowRadarNoSimdPtr ptr) 
{
    return ptr->size * (sizeof(struct Cell) + 4);
}

inline void flowradarnosimd_reset(FlowRadarNoSimdPtr ptr) 
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
            flowradarnosimd_inc_fr(ptr, (const void *)(p_key_bucket + j), *(p_value_bucket + j));
        }
    }

    memset(ptr->table, 0, flowradarnosimd_size(ptr));

    // init the simd part;
    memset(ptr->simd_ptr->table, 0, SIMD_SIZE);

#ifdef GRR_ALL
    ptr->simd_ptr->curkick = 0;
#endif

#ifdef LRU_ALL
    ptr->simd_ptr->time_now = 0;
#endif
}

FlowRadarNoSimdPtr flowradarnosimd_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);
    // the number of cells in iblt of FR: size;
    // the number of bits in bf of FR: size * 32;


    FlowRadarNoSimdPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct FlowRadarNoSimd) + 
        (size) * (sizeof(struct Cell) + 4), /* Size of elements */
        64, socket);

    

    // not size - 1 !!!
    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize;

    ptr->rowbf = ptr->table;
    ptr->rowcell = (CellPtr)(ptr->table + (size));


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


inline int  flowradarnosimd_inc_fr(FlowRadarNoSimdPtr ptr, void const * kick_key, uint32_t kick_val)
{
    uint32_t bfsize = ((ptr->size) << 5); // 32 * size, bit;
    uint32_t ibltsize = ptr->size; // cells;


    uint32_t h1 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH1) & (bfsize - 1);
    uint32_t *p1 = ptr->rowbf + (h1 >> 5);
    rte_prefetch0(p1);

    uint32_t h2 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH2 + CMS_SH1) & (bfsize - 1);
    uint32_t *p2 = ptr->rowbf + (h2 >> 5);
    rte_prefetch0(p2);

    uint32_t h3 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH3 + CMS_SH2) & (bfsize - 1);
    uint32_t *p3 = ptr->rowbf + (h3 >> 5);
    rte_prefetch0(p3);

    uint32_t h4 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH4 + CMS_SH3) & (bfsize - 1);
    uint32_t *p4 = ptr->rowbf + (h4 >> 5);
    rte_prefetch0(p4);


    int flag = 1;
    flag = (flag && (((*p1) >> (h1 & 0x1F)) & 1));
    (*p1) |= (1 << (h1 & 0x1F));

    flag = (flag && (((*p2) >> (h2 & 0x1F)) & 1));
    (*p2) |= (1 << (h2 & 0x1F));

    flag = (flag && (((*p3) >> (h3 & 0x1F)) & 1));
    (*p3) |= (1 << (h3 & 0x1F));

    flag = (flag && (((*p4) >> (h4 & 0x1F)) & 1));
    (*p4) |= (1 << (h4 & 0x1F));




    h1 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH5) & (ibltsize - 1);
    CellPtr p1_cell = ptr->rowcell + h1;
    rte_prefetch0(p1_cell);

    h2 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH6 + CMS_SH5) & (ibltsize - 1);
    CellPtr p2_cell = ptr->rowcell + h2;
    rte_prefetch0(p2_cell);

    h3 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH7 + CMS_SH5) & (ibltsize - 1);
    CellPtr p3_cell = ptr->rowcell + h3;
    rte_prefetch0(p3_cell);

    h4 = dss_hash_with_seed(kick_key, ptr->keysize, CMS_SH8 + CMS_SH5) & (ibltsize - 1);
    CellPtr p4_cell = ptr->rowcell + h4;
    rte_prefetch0(p4_cell);


    // a new flow!
    if(flag == 0)
    {
        p1_cell->flowx ^= *(uint32_t const*)kick_key;
        p1_cell->flowc ++;

        p2_cell->flowx ^= *(uint32_t const*)kick_key;
        p2_cell->flowc ++;
        
        p3_cell->flowx ^= *(uint32_t const*)kick_key;
        p3_cell->flowc ++;
        
        p4_cell->flowx ^= *(uint32_t const*)kick_key;
        p4_cell->flowc ++;
    }

    p1_cell->packetc += kick_val;
    p2_cell->packetc += kick_val;
    p3_cell->packetc += kick_val;
    p4_cell->packetc += kick_val;

    return 1;
}


inline int flowradarnosimd_inc(FlowRadarNoSimdPtr ptr, void const *key) 
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

    // const __m128i item = _mm_set1_epi32((int)flowid);

    // __m128i *keys_p = (__m128i *)p_key_bucket;


    // __m128i a_comp = _mm_cmpeq_epi32(item, keys_p[0]);
    // __m128i b_comp = _mm_cmpeq_epi32(item, keys_p[1]);
    // __m128i c_comp = _mm_cmpeq_epi32(item, keys_p[2]);
    // __m128i d_comp = _mm_cmpeq_epi32(item, keys_p[3]);

    // a_comp = _mm_packs_epi32(a_comp, b_comp);
    // c_comp = _mm_packs_epi32(c_comp, d_comp);
    // a_comp = _mm_packs_epi32(a_comp, c_comp);

    // int matched = _mm_movemask_epi8(a_comp);

    int matched = 0;
    for(int i = 0; i < 16; i++)
    {
        if(flowid == p_key_bucket[i])
        {
            matched = (1 << i);
            break;
        }
    }

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

    uint32_t ret = flowradarnosimd_inc_fr(ptr, (const void *)(p_key_bucket + curkick), p_value_bucket[curkick]);

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
    uint32_t ret = flowradarnosimd_inc_fr(ptr, (const void *)(p_key_bucket + kick_index), p_value_bucket[kick_index]);

    p_key_bucket[kick_index] = flowid;
    p_value_bucket[kick_index] = 1;


    p_clock_bucket[kick_index] = ptr->simd_ptr->time_now;

    return ret;
#endif
}

inline void flowradarnosimd_delete(FlowRadarNoSimdPtr ptr) 
{
    rte_free(ptr->simd_ptr);
    rte_free(ptr);
}

inline uint32_t flowradarnosimd_num_searches(FlowRadarNoSimdPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_flowradarnosimd_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchFlowRadarNoSimdPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchFlowRadarNoSimd), 64, socket); 

    module->_m.execute = simdbatch_flowradarnosimd_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->flowradarnosimd_ptr1 = flowradarnosimd_create(size, keysize, elsize, socket);
    module->flowradarnosimd_ptr2 = flowradarnosimd_create(size, keysize, elsize, socket);

    module->flowradarnosimd = module->flowradarnosimd_ptr1;

    return (ModulePtr)module;
}

void simdbatch_flowradarnosimd_delete(ModulePtr module_) 
{
    ModuleSimdBatchFlowRadarNoSimdPtr module = (ModuleSimdBatchFlowRadarNoSimdPtr)module_;

    flowradarnosimd_delete(module->flowradarnosimd_ptr1);
    flowradarnosimd_delete(module->flowradarnosimd_ptr2);

    rte_free(module);
}

inline void simdbatch_flowradarnosimd_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchFlowRadarNoSimdPtr module = (ModuleSimdBatchFlowRadarNoSimdPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    FlowRadarNoSimdPtr flowradarnosimd = module->flowradarnosimd;

    /* Prefetch flowradarnosimd entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        flowradarnosimd_inc(flowradarnosimd, (pkt + 26));
    }
}

inline void simdbatch_flowradarnosimd_reset(ModulePtr module_) 
{
    ModuleSimdBatchFlowRadarNoSimdPtr module = (ModuleSimdBatchFlowRadarNoSimdPtr)module_;
    FlowRadarNoSimdPtr prev = module->flowradarnosimd;

    if (module->flowradarnosimd == module->flowradarnosimd_ptr1) 
    {
        module->flowradarnosimd = module->flowradarnosimd_ptr2;
    } 
    else 
    {
        module->flowradarnosimd = module->flowradarnosimd_ptr1;
    }

    module->stats_search += flowradarnosimd_num_searches(prev);
    flowradarnosimd_reset(prev);
}

inline void simdbatch_flowradarnosimd_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchFlowRadarNoSimdPtr module = (ModuleSimdBatchFlowRadarNoSimdPtr)module_;
    module->stats_search += flowradarnosimd_num_searches(module->flowradarnosimd);
    fprintf(f, "SimdBatch::FlowRadarNoSimd::SearchLoad\t%u\n", module->stats_search);
}


