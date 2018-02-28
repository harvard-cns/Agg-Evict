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

#include "spacesavingsimd.h"




inline void spacesavingsimd_reset(SpaceSavingSimdPtr ptr) 
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
            spacesavingsimd_inc_ss(ptr, (const void *)(p_key_bucket + j), *(p_value_bucket + j));
        }
    }


    FILE *file_topk = fopen("file_topk_simd.txt", "a");
    NodePtr heap = ptr->nodes;
    uint32_t heap_element_num = ptr->now_element;
    for(uint32_t i = 0; i < heap_element_num; i++)
    {
        fprintf(file_topk, "%u\n", (uint32_t)heap[i].flowid);
    }
    fprintf(file_topk, "--------------------------2M-------------------------\n");
    fflush(file_topk);
    fclose(file_topk);





    ptr->now_element = 0;
    memset(ptr->table, 0, (ptr->size) * sizeof(struct Node));

    // init the simd part;
    memset(ptr->simd_ptr->table, 0, SIMD_SIZE);
 
#ifdef GRR_ALL
    ptr->simd_ptr->curkick = 0;
#endif

#ifdef LRU_ALL
    ptr->simd_ptr->time_now = 0;
#endif
}

SpaceSavingSimdPtr spacesavingsimd_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    // size is the number of elements in the stream-summary;
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    SpaceSavingSimdPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct SpaceSavingSimd) + 
        (size + 1) * sizeof(struct Node), /* Size of elements */
        64, socket);

    
    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys


    // for stream-summary;
    ptr->now_element = 0;
    ptr->nodes = (NodePtr)ptr->table;

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

inline void swap_32_ss_simd(uint32_t *a, uint32_t *b)
{
    uint32_t t;
    t = *a;
    *a = *b;
    *b = t;
}

inline void swap_64_ss_simd(uint64_t *a, uint64_t *b)
{
    uint64_t t;
    t = *a;
    *a = *b;
    *b = t;
}

inline void heap_adjust_down_ss_simd(SpaceSavingSimdPtr ptr, uint32_t i) 
{
    NodePtr heap = ptr->nodes;

    uint32_t heap_element_num = ptr->now_element;
    
    while (i < heap_element_num / 2) 
    {
        uint32_t l_child = 2 * i + 1;
        uint32_t r_child = 2 * i + 2;
        uint32_t larger_one = i;
        if (l_child < heap_element_num && heap[l_child].freq < heap[larger_one].freq) 
        {
            larger_one = l_child;
        }
        if (r_child < heap_element_num && heap[r_child].freq < heap[larger_one].freq) 
        {
            larger_one = r_child;
        }
        if (larger_one != i) 
        {
            swap_64_ss_simd((uint64_t*)&(heap[i]), (uint64_t*)&(heap[larger_one]));
    
            heap_adjust_down_ss_simd(ptr, larger_one);
        } 
        else 
        {
            break;
        }
    }
}
inline void heap_adjust_up_ss_simd(SpaceSavingSimdPtr ptr, uint32_t i) 
{
    NodePtr heap = ptr->nodes;
    
    while (i > 1) 
    {
        uint32_t parent = (i - 1) / 2;
        if (heap[parent].freq <= heap[i].freq) 
        {
            break;
        }
        swap_64_ss_simd((uint64_t*)&(heap[i]), (uint64_t*)&(heap[parent]));

        i = parent;
    }
}

inline int find_in_heap_simd(SpaceSavingSimdPtr ptr, uint32_t key)
{
    NodePtr heap = ptr->nodes;
    uint32_t heap_element_num = ptr->now_element;
    for(uint32_t i = 0; i < heap_element_num; i++)
    {
        if(heap[i].flowid == key)
        {
            return i;
        }
    }
    return -1;
}
inline int spacesavingsimd_inc_ss(SpaceSavingSimdPtr ptr, void const * kick_key, uint32_t kick_val)
{
     rte_atomic32_inc(&ptr->stats_search);
     NodePtr heap = ptr->nodes;
    uint32_t flowid = *(uint32_t const *)kick_key;

    int ret = find_in_heap_simd(ptr, *(uint32_t const *)kick_key);

    if(ret != -1)
    {
        heap[ret].freq += kick_val;
        heap_adjust_down_ss_simd(ptr, ret);
    }
    else if(ptr->now_element < ptr->size)
    {
        heap[ptr->now_element].flowid = flowid;
        heap[ptr->now_element].freq = kick_val;
        
        (ptr->now_element)++;

        heap_adjust_up_ss_simd(ptr, (ptr->now_element) - 1);
    }
    else
    {
        heap[0].flowid = flowid;
        heap[0].freq += kick_val;
        heap_adjust_down_ss_simd(ptr, 0);
    }
    return 1;
}


inline int spacesavingsimd_inc(SpaceSavingSimdPtr ptr, void const *key) 
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

    uint32_t ret = spacesavingsimd_inc_ss(ptr, (const void *)(p_key_bucket + curkick), p_value_bucket[curkick]);

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
    uint32_t ret = spacesavingsimd_inc_ss(ptr, (const void *)(p_key_bucket + kick_index), p_value_bucket[kick_index]);

    p_key_bucket[kick_index] = flowid;
    p_value_bucket[kick_index] = 1;


    p_clock_bucket[kick_index] = ptr->simd_ptr->time_now;

    return ret;
#endif

}

inline void spacesavingsimd_delete(SpaceSavingSimdPtr ptr) 
{
    rte_free(ptr->simd_ptr);
    rte_free(ptr);
}

inline uint32_t spacesavingsimd_num_searches(SpaceSavingSimdPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_spacesavingsimd_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchSpaceSavingSimdPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchSpaceSavingSimd), 64, socket); 

    module->_m.execute = simdbatch_spacesavingsimd_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->spacesavingsimd_ptr1 = spacesavingsimd_create(size, keysize, elsize, socket);
    module->spacesavingsimd_ptr2 = spacesavingsimd_create(size, keysize, elsize, socket);

    module->spacesavingsimd = module->spacesavingsimd_ptr1;

    return (ModulePtr)module;
}

void simdbatch_spacesavingsimd_delete(ModulePtr module_) 
{
    ModuleSimdBatchSpaceSavingSimdPtr module = (ModuleSimdBatchSpaceSavingSimdPtr)module_;

    spacesavingsimd_delete(module->spacesavingsimd_ptr1);
    spacesavingsimd_delete(module->spacesavingsimd_ptr2);

    rte_free(module);
}

inline void simdbatch_spacesavingsimd_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchSpaceSavingSimdPtr module = (ModuleSimdBatchSpaceSavingSimdPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    SpaceSavingSimdPtr spacesavingsimd = module->spacesavingsimd;

    /* Prefetch spacesavingsimd entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        spacesavingsimd_inc(spacesavingsimd, (pkt + 26));
    }
}

inline void simdbatch_spacesavingsimd_reset(ModulePtr module_) 
{
    ModuleSimdBatchSpaceSavingSimdPtr module = (ModuleSimdBatchSpaceSavingSimdPtr)module_;
    SpaceSavingSimdPtr prev = module->spacesavingsimd;

    if (module->spacesavingsimd == module->spacesavingsimd_ptr1) 
    {
        module->spacesavingsimd = module->spacesavingsimd_ptr2;
    } 
    else 
    {
        module->spacesavingsimd = module->spacesavingsimd_ptr1;
    }

    module->stats_search += spacesavingsimd_num_searches(prev);
    spacesavingsimd_reset(prev);
}

inline void simdbatch_spacesavingsimd_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchSpaceSavingSimdPtr module = (ModuleSimdBatchSpaceSavingSimdPtr)module_;
    module->stats_search += spacesavingsimd_num_searches(module->spacesavingsimd);
    fprintf(f, "SimdBatch::SpaceSavingSimd::SearchLoad\t%u\n", module->stats_search);
}


