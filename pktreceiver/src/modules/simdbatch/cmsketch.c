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

#include "cmsketch.h"




inline static uint32_t cmsketch_size(CMSketchPtr ptr) 
{
    return ptr->size * 4 + 4;
}

inline void cmsketch_reset(CMSketchPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
    memset(ptr->table, 0, cmsketch_size(ptr) * (ptr->elsize) * sizeof(uint32_t));
}

CMSketchPtr cmsketch_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    CMSketchPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct CMSketch) + 
        4 * (size) * (elsize) * sizeof(uint32_t), /* Size of elements */
        64, socket);


    // for (h & ptr->size) to function as %;     
    ptr->size = size - 1;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->row1 = ptr->table;
    ptr->row2 = ptr->table + (size * elsize);
    ptr->row3 = ptr->table + (size * 2 * elsize);
    ptr->row4 = ptr->table + (size * 3 * elsize);

    return ptr;
}




inline static uint32_t cmsketch_offset(CMSketchPtr ptr, uint32_t h) 
{
    return ((h & ptr->size) * ptr->elsize);
}

inline int cmsketch_inc(CMSketchPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

    uint32_t h1 = dss_hash_with_seed(key, ptr->keysize, CMS_SH1);
    uint32_t *p1 = ptr->row1 + cmsketch_offset(ptr, h1);
    rte_prefetch0(p1);

    uint32_t h2 = dss_hash_with_seed(key, ptr->keysize, CMS_SH2 + CMS_SH1);
    uint32_t *p2 = ptr->row2 + cmsketch_offset(ptr, h2);
    rte_prefetch0(p2);

    uint32_t h3 = dss_hash_with_seed(key, ptr->keysize, CMS_SH3 + CMS_SH2);
    uint32_t *p3 = ptr->row3 + cmsketch_offset(ptr, h3);
    rte_prefetch0(p3);

    uint32_t h4 = dss_hash_with_seed(key, ptr->keysize, CMS_SH4 + CMS_SH3);
    uint32_t *p4 = ptr->row4 + cmsketch_offset(ptr, h4);
    rte_prefetch0(p4);


    (*p1)++;
    (*p2)++;
    (*p3)++;
    (*p4)++;

    return 1;
}

inline void cmsketch_delete(CMSketchPtr ptr) 
{
    rte_free(ptr);
}

inline uint32_t cmsketch_num_searches(CMSketchPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_cmsketch_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchCMSketchPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchCMSketch), 64, socket); 

    module->_m.execute = simdbatch_cmsketch_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->cmsketch_ptr1 = cmsketch_create(size, keysize, elsize, socket);
    module->cmsketch_ptr2 = cmsketch_create(size, keysize, elsize, socket);

    module->cmsketch = module->cmsketch_ptr1;

    return (ModulePtr)module;
}

void simdbatch_cmsketch_delete(ModulePtr module_) 
{
    ModuleSimdBatchCMSketchPtr module = (ModuleSimdBatchCMSketchPtr)module_;

    cmsketch_delete(module->cmsketch_ptr1);
    cmsketch_delete(module->cmsketch_ptr2);

    rte_free(module);
}

inline void simdbatch_cmsketch_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchCMSketchPtr module = (ModuleSimdBatchCMSketchPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    CMSketchPtr cmsketch = module->cmsketch;

    /* Prefetch cmsketch entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        cmsketch_inc(cmsketch, (pkt + 26));
    }
}

inline void simdbatch_cmsketch_reset(ModulePtr module_) 
{
    ModuleSimdBatchCMSketchPtr module = (ModuleSimdBatchCMSketchPtr)module_;
    CMSketchPtr prev = module->cmsketch;

    if (module->cmsketch == module->cmsketch_ptr1) 
    {
        module->cmsketch = module->cmsketch_ptr2;
    } 
    else 
    {
        module->cmsketch = module->cmsketch_ptr1;
    }

    module->stats_search += cmsketch_num_searches(prev);
    cmsketch_reset(prev);
}

inline void simdbatch_cmsketch_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchCMSketchPtr module = (ModuleSimdBatchCMSketchPtr)module_;
    module->stats_search += cmsketch_num_searches(module->cmsketch);
    fprintf(f, "SimdBatch::CMSketch::SearchLoad\t%u\n", module->stats_search);
}


