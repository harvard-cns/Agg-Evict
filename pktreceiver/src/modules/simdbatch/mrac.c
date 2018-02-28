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

#include "mrac.h"




inline static uint32_t mrac_size(MRACPtr ptr) 
{
    return ptr->size * 4;
}

inline void mrac_reset(MRACPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
    memset(ptr->table, 0, mrac_size(ptr) * (ptr->elsize) * sizeof(uint32_t));
}

MRACPtr mrac_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);
    // size is the number of counters in each row (four row);


    MRACPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct MRAC) + 
        4 * (size) * (elsize) * sizeof(uint32_t), /* Size of elements */
        64, socket);


    ptr->size = size;
    ptr->M = size << 3;
    // virtual size

    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys


    ptr->fence12 = ptr->M / 2;
    ptr->fence23 = ptr->fence12 + ptr->fence12 / 2;
    ptr->fence34 = ptr->fence23 + ptr->fence23 / 2;


    ptr->row1 = ptr->table;
    ptr->row2 = ptr->table + (size * elsize);
    ptr->row3 = ptr->table + (size * 2 * elsize);
    ptr->row4 = ptr->table + (size * 3 * elsize);

    return ptr;
}




inline int mrac_inc(MRACPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

    uint32_t hv = dss_hash_with_seed(key, ptr->keysize, CMS_SH1) & (ptr->M - 1);

    if(hv < ptr->fence12)
    {
        uint32_t *pos = ptr->row1 + (hv & (ptr->size - 1));
        rte_prefetch0(pos);
        (*(pos)) ++;
    }
    else if(hv < ptr->fence23)
    {
        uint32_t *pos = ptr->row2 + (hv & (ptr->size - 1));
        rte_prefetch0(pos);
        (*(pos)) ++;
    }
    else if(hv < ptr->fence34)
    {
        uint32_t *pos = ptr->row3 + (hv & (ptr->size - 1));
        rte_prefetch0(pos);
        (*(pos)) ++;
    }
    else
    {
        uint32_t *pos = ptr->row4 + (hv & (ptr->size - 1));
        rte_prefetch0(pos);
        (*(pos)) ++;
    }

    return 1;
}

inline void mrac_delete(MRACPtr ptr) 
{
    rte_free(ptr);
}

inline uint32_t mrac_num_searches(MRACPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_mrac_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchMRACPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchMRAC), 64, socket); 

    module->_m.execute = simdbatch_mrac_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->mrac_ptr1 = mrac_create(size, keysize, elsize, socket);
    module->mrac_ptr2 = mrac_create(size, keysize, elsize, socket);

    module->mrac = module->mrac_ptr1;

    return (ModulePtr)module;
}

void simdbatch_mrac_delete(ModulePtr module_) 
{
    ModuleSimdBatchMRACPtr module = (ModuleSimdBatchMRACPtr)module_;

    mrac_delete(module->mrac_ptr1);
    mrac_delete(module->mrac_ptr2);

    rte_free(module);
}

inline void simdbatch_mrac_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchMRACPtr module = (ModuleSimdBatchMRACPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    MRACPtr mrac = module->mrac;

    /* Prefetch mrac entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        mrac_inc(mrac, (pkt + 26));
    }
}

inline void simdbatch_mrac_reset(ModulePtr module_) 
{
    ModuleSimdBatchMRACPtr module = (ModuleSimdBatchMRACPtr)module_;
    MRACPtr prev = module->mrac;

    if (module->mrac == module->mrac_ptr1) 
    {
        module->mrac = module->mrac_ptr2;
    } 
    else 
    {
        module->mrac = module->mrac_ptr1;
    }

    module->stats_search += mrac_num_searches(prev);
    mrac_reset(prev);
}

inline void simdbatch_mrac_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchMRACPtr module = (ModuleSimdBatchMRACPtr)module_;
    module->stats_search += mrac_num_searches(module->mrac);
    fprintf(f, "SimdBatch::MRAC::SearchLoad\t%u\n", module->stats_search);
}


