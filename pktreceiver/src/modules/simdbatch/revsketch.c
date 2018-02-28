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

#include "revsketch.h"


inline void revsketch_reset(RevSketchPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
    memset(ptr->table, 0, 4 * ptr->size * sizeof(uint32_t));
}

RevSketchPtr revsketch_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    RevSketchPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct RevSketch) + 
        4 * size * sizeof(uint32_t), /* Size of elements */
        64, socket);


    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->row1 = ptr->table;
    ptr->row2 = ptr->table + (size * elsize);
    ptr->row3 = ptr->table + (size * 2 * elsize);
    ptr->row4 = ptr->table + (size * 3 * elsize);

    return ptr;
}



inline int revsketch_inc(RevSketchPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

    uint32_t h1 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH1) & 0xF;
    uint32_t h2 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH2 + CMS_SH1) & 0xF;
    uint32_t h3 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH3 + CMS_SH2) & 0xF;
    uint32_t h4 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH4 + CMS_SH3) & 0xF;

    uint32_t *p1 = ptr->row1 + (h1 | (h2 << 4) | (h3 << 8) | (h4 << 12));
    rte_prefetch0(p1);


    h1 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH5) & 0xF;
    h2 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH6 + CMS_SH5) & 0xF;
    h3 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH7 + CMS_SH6) & 0xF;
    h4 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH8 + CMS_SH7) & 0xF;

    uint32_t *p2 = ptr->row2 + (h1 | (h2 << 4) | (h3 << 8) | (h4 << 12));
    rte_prefetch0(p2);


    h1 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH9) & 0xF;
    h2 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH10 + CMS_SH9) & 0xF;
    h3 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH11 + CMS_SH10) & 0xF;
    h4 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH12 + CMS_SH11) & 0xF;

    uint32_t *p3 = ptr->row3 + (h1 | (h2 << 4) | (h3 << 8) | (h4 << 12));
    rte_prefetch0(p3);


    h1 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH13) & 0xF;
    h2 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH14 + CMS_SH13) & 0xF;
    h3 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH15 + CMS_SH14) & 0xF;
    h4 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH16 + CMS_SH15) & 0xF;

    uint32_t *p4 = ptr->row4 + (h1 | (h2 << 4) | (h3 << 8) | (h4 << 12));
    rte_prefetch0(p4);


    (*p1)++;
    (*p2)++;
    (*p3)++;
    (*p4)++;

    return 1;
}

inline void revsketch_delete(RevSketchPtr ptr) 
{
    rte_free(ptr);
}

inline uint32_t revsketch_num_searches(RevSketchPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_revsketch_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchRevSketchPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchRevSketch), 64, socket); 

    module->_m.execute = simdbatch_revsketch_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->revsketch_ptr1 = revsketch_create(size, keysize, elsize, socket);
    module->revsketch_ptr2 = revsketch_create(size, keysize, elsize, socket);

    module->revsketch = module->revsketch_ptr1;

    return (ModulePtr)module;
}

void simdbatch_revsketch_delete(ModulePtr module_) 
{
    ModuleSimdBatchRevSketchPtr module = (ModuleSimdBatchRevSketchPtr)module_;

    revsketch_delete(module->revsketch_ptr1);
    revsketch_delete(module->revsketch_ptr2);

    rte_free(module);
}

inline void simdbatch_revsketch_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchRevSketchPtr module = (ModuleSimdBatchRevSketchPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    RevSketchPtr revsketch = module->revsketch;

    /* Prefetch revsketch entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        revsketch_inc(revsketch, (pkt + 26));
    }
}

inline void simdbatch_revsketch_reset(ModulePtr module_) 
{
    ModuleSimdBatchRevSketchPtr module = (ModuleSimdBatchRevSketchPtr)module_;
    RevSketchPtr prev = module->revsketch;

    if (module->revsketch == module->revsketch_ptr1) 
    {
        module->revsketch = module->revsketch_ptr2;
    } 
    else 
    {
        module->revsketch = module->revsketch_ptr1;
    }

    module->stats_search += revsketch_num_searches(prev);
    revsketch_reset(prev);
}

inline void simdbatch_revsketch_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchRevSketchPtr module = (ModuleSimdBatchRevSketchPtr)module_;
    module->stats_search += revsketch_num_searches(module->revsketch);
    fprintf(f, "SimdBatch::RevSketch::SearchLoad\t%u\n", module->stats_search);
}


