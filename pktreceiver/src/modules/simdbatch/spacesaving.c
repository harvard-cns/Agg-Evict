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

#include "spacesaving.h"




inline void spacesaving_reset(SpaceSavingPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
    
    FILE *file_topk = fopen("file_topk.txt", "a");
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

}

SpaceSavingPtr spacesaving_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    // size is the number of elements in the stream-summary;
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    SpaceSavingPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct SpaceSaving) + 
        (size) * sizeof(struct Node), /* Size of elements */
        64, socket);


    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->nodes = (NodePtr)ptr->table;
    ptr->now_element = 0;

    return ptr;
}


inline void swap_32_ss(uint32_t *a, uint32_t *b)
{
    uint32_t t;
    t = *a;
    *a = *b;
    *b = t;
}

inline void swap_64_ss(uint64_t *a, uint64_t *b)
{
    uint64_t t;
    t = *a;
    *a = *b;
    *b = t;
}

inline void heap_adjust_down_ss(SpaceSavingPtr ptr, uint32_t i) 
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
            swap_64_ss((uint64_t*)&(heap[i]), (uint64_t*)&(heap[larger_one]));
            heap_adjust_down_ss(ptr, larger_one);
        } 
        else 
        {
            break;
        }
    }
}
inline void heap_adjust_up_ss(SpaceSavingPtr ptr, uint32_t i) 
{
    NodePtr heap = ptr->nodes;
    
    while (i > 1) 
    {
        uint32_t parent = (i - 1) / 2;
        if (heap[parent].freq <= heap[i].freq) 
        {
            break;
        }
        swap_64_ss((uint64_t*)&(heap[i]), (uint64_t*)&(heap[parent]));
            
        i = parent;
    }
}
inline int find_in_heap(SpaceSavingPtr ptr, uint32_t key)
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
inline int spacesaving_inc(SpaceSavingPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);
    NodePtr heap = ptr->nodes;
    uint32_t flowid = *(uint32_t const *)key;

    int ret = find_in_heap(ptr, *(uint32_t const *)key);
    if(ret != -1)
    {
        heap[ret].freq += 1;
        heap_adjust_down_ss(ptr, ret);
    }
    else if(ptr->now_element < ptr->size)
    {
        heap[ptr->now_element].flowid = flowid;
        heap[ptr->now_element].freq = 1;
        
        (ptr->now_element)++;

        heap_adjust_up_ss(ptr, (ptr->now_element) - 1);
    }
    else
    {
        heap[0].flowid = flowid;
        heap[0].freq += 1;
        heap_adjust_down_ss(ptr, 0);
    }
    return 1;
}

inline void spacesaving_delete(SpaceSavingPtr ptr) 
{
    rte_free(ptr);
}

inline uint32_t spacesaving_num_searches(SpaceSavingPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_spacesaving_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchSpaceSavingPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchSpaceSaving), 64, socket); 

    module->_m.execute = simdbatch_spacesaving_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->spacesaving_ptr1 = spacesaving_create(size, keysize, elsize, socket);
    module->spacesaving_ptr2 = spacesaving_create(size, keysize, elsize, socket);

    module->spacesaving = module->spacesaving_ptr1;

    return (ModulePtr)module;
}

void simdbatch_spacesaving_delete(ModulePtr module_) 
{
    ModuleSimdBatchSpaceSavingPtr module = (ModuleSimdBatchSpaceSavingPtr)module_;

    spacesaving_delete(module->spacesaving_ptr1);
    spacesaving_delete(module->spacesaving_ptr2);

    rte_free(module);
}

inline void simdbatch_spacesaving_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchSpaceSavingPtr module = (ModuleSimdBatchSpaceSavingPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    SpaceSavingPtr spacesaving = module->spacesaving;

    /* Prefetch spacesaving entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        spacesaving_inc(spacesaving, (pkt + 26));
    }
}

inline void simdbatch_spacesaving_reset(ModulePtr module_) 
{
    ModuleSimdBatchSpaceSavingPtr module = (ModuleSimdBatchSpaceSavingPtr)module_;
    SpaceSavingPtr prev = module->spacesaving;

    if (module->spacesaving == module->spacesaving_ptr1) 
    {
        module->spacesaving = module->spacesaving_ptr2;
    } 
    else 
    {
        module->spacesaving = module->spacesaving_ptr1;
    }

    module->stats_search += spacesaving_num_searches(prev);
    spacesaving_reset(prev);
}

inline void simdbatch_spacesaving_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchSpaceSavingPtr module = (ModuleSimdBatchSpaceSavingPtr)module_;
    module->stats_search += spacesaving_num_searches(module->spacesaving);
    fprintf(f, "SimdBatch::SpaceSaving::SearchLoad\t%u\n", module->stats_search);
}


