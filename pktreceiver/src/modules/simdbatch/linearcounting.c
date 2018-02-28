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

#include "linearcounting.h"



inline void linearcounting_reset(LinearCountingPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
    memset(ptr->table, 0, 4 * ptr->size * sizeof(uint32_t));
}

LinearCountingPtr linearcounting_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    // size is the number of uint32_t in one row;
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    LinearCountingPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct LinearCounting) + 
        4 * (size) * sizeof(uint32_t), /* Size of elements */
        64, socket);


    // not size - 1 here!!!   
    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->row1 = ptr->table;
    ptr->row2 = ptr->table + (size);
    ptr->row3 = ptr->table + (size * 2);
    ptr->row4 = ptr->table + (size * 3);

    return ptr;
}





inline int linearcounting_inc(LinearCountingPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

    uint32_t rowsize = (ptr->size) << 5;


    uint32_t h1 = dss_hash_with_seed(key, ptr->keysize, CMS_SH1) & (rowsize - 1);
    uint32_t *p1 = ptr->row1 + (h1 >> 5);
    rte_prefetch0(p1);

    uint32_t h2 = dss_hash_with_seed(key, ptr->keysize, CMS_SH2 + CMS_SH1) & (rowsize - 1);
    uint32_t *p2 = ptr->row2 + (h2 >> 5);
    rte_prefetch0(p2);

    uint32_t h3 = dss_hash_with_seed(key, ptr->keysize, CMS_SH3 + CMS_SH2) & (rowsize - 1);
    uint32_t *p3 = ptr->row3 + (h3 >> 5);
    rte_prefetch0(p3);

    uint32_t h4 = dss_hash_with_seed(key, ptr->keysize, CMS_SH4 + CMS_SH3) & (rowsize - 1);
    uint32_t *p4 = ptr->row4 + (h4 >> 5);
    rte_prefetch0(p4);


    (*p1) |= (1 << (h1 & 0x1F));
    (*p2) |= (1 << (h2 & 0x1F));
    (*p3) |= (1 << (h3 & 0x1F));
    (*p4) |= (1 << (h4 & 0x1F));

    return 1;
}

inline void linearcounting_delete(LinearCountingPtr ptr) 
{
    rte_free(ptr);
}

inline uint32_t linearcounting_num_searches(LinearCountingPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_linearcounting_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchLinearCountingPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchLinearCounting), 64, socket); 

    module->_m.execute = simdbatch_linearcounting_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->linearcounting_ptr1 = linearcounting_create(size, keysize, elsize, socket);
    module->linearcounting_ptr2 = linearcounting_create(size, keysize, elsize, socket);

    module->linearcounting = module->linearcounting_ptr1;

    return (ModulePtr)module;
}

void simdbatch_linearcounting_delete(ModulePtr module_) 
{
    ModuleSimdBatchLinearCountingPtr module = (ModuleSimdBatchLinearCountingPtr)module_;

    linearcounting_delete(module->linearcounting_ptr1);
    linearcounting_delete(module->linearcounting_ptr2);

    rte_free(module);
}

inline void simdbatch_linearcounting_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchLinearCountingPtr module = (ModuleSimdBatchLinearCountingPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    LinearCountingPtr linearcounting = module->linearcounting;

    /* Prefetch linearcounting entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        linearcounting_inc(linearcounting, (pkt + 26));
    }
}

inline void simdbatch_linearcounting_reset(ModulePtr module_) 
{
    ModuleSimdBatchLinearCountingPtr module = (ModuleSimdBatchLinearCountingPtr)module_;
    LinearCountingPtr prev = module->linearcounting;

    if (module->linearcounting == module->linearcounting_ptr1) 
    {
        module->linearcounting = module->linearcounting_ptr2;
    } 
    else 
    {
        module->linearcounting = module->linearcounting_ptr1;
    }

    module->stats_search += linearcounting_num_searches(prev);
    linearcounting_reset(prev);
}

inline void simdbatch_linearcounting_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchLinearCountingPtr module = (ModuleSimdBatchLinearCountingPtr)module_;
    module->stats_search += linearcounting_num_searches(module->linearcounting);
    fprintf(f, "SimdBatch::linearcounting::SearchLoad\t%u\n", module->stats_search);
}


