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

#include "univmon.h"



inline void univmon_reset(UnivMonPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
    count_heap_reset(ptr->CH1);
    count_heap_reset(ptr->CH2);
    count_heap_reset(ptr->CH3);
    count_heap_reset(ptr->CH4);
}

UnivMonPtr univmon_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    UnivMonPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct UnivMon), /* Size of elements */
        64, socket);


    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->CH1 = count_heap_create(size, 128);
    ptr->CH2 = count_heap_create(size / 2, 128);
    ptr->CH3 = count_heap_create(size / 4, 128);
    ptr->CH4 = count_heap_create(size / 8, 128);

    return ptr;
}



inline int univmon_inc(UnivMonPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

    count_heap_insert(ptr->CH1, *(uint32_t const*)key, 1);

    uint32_t h1 = dss_hash_with_seed(key, ptr->keysize, CMS_SH1) & 1;
    if(h1 == 1)
    {
        count_heap_insert(ptr->CH2, *(uint32_t const*)key, 1);
        return 0;
    }

    uint32_t h2 = dss_hash_with_seed(key, ptr->keysize, CMS_SH2 + CMS_SH1) & 1;
    if(h2 == 1)
    {
        count_heap_insert(ptr->CH3, *(uint32_t const*)key, 1);
        return 0;
    }

    uint32_t h3 = dss_hash_with_seed(key, ptr->keysize, CMS_SH3 + CMS_SH2) & 1;
    if(h3 == 1)
    {
        count_heap_insert(ptr->CH4, *(uint32_t const*)key, 1);
        return 0;
    }
    return 1;
}

inline void univmon_delete(UnivMonPtr ptr) 
{
    count_heap_delete(ptr->CH1);
    count_heap_delete(ptr->CH2);
    count_heap_delete(ptr->CH3);
    count_heap_delete(ptr->CH4);

    rte_free(ptr);
}

inline uint32_t univmon_num_searches(UnivMonPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_univmon_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchUnivMonPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchUnivMon), 64, socket); 

    module->_m.execute = simdbatch_univmon_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->univmon_ptr1 = univmon_create(size, keysize, elsize, socket);
    module->univmon_ptr2 = univmon_create(size, keysize, elsize, socket);

    module->univmon = module->univmon_ptr1;

    return (ModulePtr)module;
}

void simdbatch_univmon_delete(ModulePtr module_) 
{
    ModuleSimdBatchUnivMonPtr module = (ModuleSimdBatchUnivMonPtr)module_;

    univmon_delete(module->univmon_ptr1);
    univmon_delete(module->univmon_ptr2);

    rte_free(module);
}

inline void simdbatch_univmon_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchUnivMonPtr module = (ModuleSimdBatchUnivMonPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    UnivMonPtr univmon = module->univmon;

    /* Prefetch univmon entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        univmon_inc(univmon, (pkt + 26));
    }
}

inline void simdbatch_univmon_reset(ModulePtr module_) 
{
    ModuleSimdBatchUnivMonPtr module = (ModuleSimdBatchUnivMonPtr)module_;
    UnivMonPtr prev = module->univmon;

    if (module->univmon == module->univmon_ptr1) 
    {
        module->univmon = module->univmon_ptr2;
    } 
    else 
    {
        module->univmon = module->univmon_ptr1;
    }

    module->stats_search += univmon_num_searches(prev);
    univmon_reset(prev);
}

inline void simdbatch_univmon_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchUnivMonPtr module = (ModuleSimdBatchUnivMonPtr)module_;
    module->stats_search += univmon_num_searches(module->univmon);
    fprintf(f, "SimdBatch::UnivMon::SearchLoad\t%u\n", module->stats_search);
}


