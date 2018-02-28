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

#include "twolevel.h"


inline void twolevel_reset(TwoLevelPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
    memset(ptr->table, 0, 4 * ptr->size * sizeof(struct BitCell) + 4 * 4096 * sizeof(struct BitCell));
}

TwoLevelPtr twolevel_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    TwoLevelPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct TwoLevel) + 
        4 * size * sizeof(struct BitCell) + 4 * 4096 * sizeof(struct BitCell), /* Size of elements */
        64, socket);


    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->rowrev1 = (BitCellPtr)ptr->table;
    ptr->rowrev2 = (BitCellPtr)(ptr->table + 4096 * 1 * 3);
    ptr->rowrev3 = (BitCellPtr)(ptr->table + 4096 * 2 * 3);
    ptr->rowrev4 = (BitCellPtr)(ptr->table + 4096 * 3 * 3);

    ptr->rowcm1 = (BitCellPtr)(ptr->table + 4096 * 4 * 3);
    ptr->rowcm2 = (BitCellPtr)(ptr->table + 4096 * 4 * 3 + size * 1 * 3);
    ptr->rowcm3 = (BitCellPtr)(ptr->table + 4096 * 4 * 3 + size * 2 * 3);
    ptr->rowcm4 = (BitCellPtr)(ptr->table + 4096 * 4 * 3 + size * 3 * 3);

    return ptr;
}

inline void cell_inc(BitCellPtr ptr, void const * key)
{
    uint32_t h1 = dss_hash_with_seed(key, 1, CMS_SH5) % 96;
    uint32_t a = h1 >> 5;
    uint32_t b = h1 & 0x1F;

    ptr->cell[a] |= (1 << b);
}


inline int twolevel_inc(TwoLevelPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

    uint32_t h1 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH1) & 0x7;
    uint32_t h2 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH2 + CMS_SH1) & 0x7;
    uint32_t h3 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH3 + CMS_SH2) & 0x7;
    uint32_t h4 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH4 + CMS_SH3) & 0x7;

    BitCellPtr p1 = ptr->rowrev1 + (h1 | (h2 << 3) | (h3 << 6) | (h4 << 9));
    rte_prefetch0(p1);


    h1 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH5) & 0x7;
    h2 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH6 + CMS_SH5) & 0x7;
    h3 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH7 + CMS_SH6) & 0x7;
    h4 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH8 + CMS_SH7) & 0x7;

    BitCellPtr p2 = ptr->rowrev2 + (h1 | (h2 << 3) | (h3 << 6) | (h4 << 9));
    rte_prefetch0(p2);


    h1 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH9) & 0x7;
    h2 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH10 + CMS_SH9) & 0x7;
    h3 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH11 + CMS_SH10) & 0x7;
    h4 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH12 + CMS_SH11) & 0x7;

    BitCellPtr p3 = ptr->rowrev3 + (h1 | (h2 << 3) | (h3 << 6) | (h4 << 9));
    rte_prefetch0(p3);


    h1 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH13) & 0x7;
    h2 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH14 + CMS_SH13) & 0x7;
    h3 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH15 + CMS_SH14) & 0x7;
    h4 = dss_hash_with_seed_rev(key, ptr->keysize, CMS_SH16 + CMS_SH15) & 0x7;

    BitCellPtr p4 = ptr->rowrev4 + (h1 | (h2 << 3) | (h3 << 6) | (h4 << 9));
    rte_prefetch0(p4);

    cell_inc(p1, ((uint32_t const*)key) + 1);
    cell_inc(p2, ((uint32_t const*)key) + 1);
    cell_inc(p3, ((uint32_t const*)key) + 1);
    cell_inc(p4, ((uint32_t const*)key) + 1);



    h1 = dss_hash_with_seed(key, ptr->keysize, CMS_SH1) & (ptr->size - 1);
    p1 = ptr->rowcm1 + h1;
    rte_prefetch0(p1);

    h2 = dss_hash_with_seed(key, ptr->keysize, CMS_SH2 + CMS_SH1) & (ptr->size - 1);
    p2 = ptr->rowcm2 + h2;
    rte_prefetch0(p2);

    h3 = dss_hash_with_seed(key, ptr->keysize, CMS_SH3 + CMS_SH2) & (ptr->size - 1);
    p3 = ptr->rowcm3 + h3;
    rte_prefetch0(p3);

    h4 = dss_hash_with_seed(key, ptr->keysize, CMS_SH4 + CMS_SH3) & (ptr->size - 1);
    p4 = ptr->rowcm4 + h3;
    rte_prefetch0(p4);

    cell_inc(p1, ((uint32_t const*)key) + 1);
    cell_inc(p2, ((uint32_t const*)key) + 1);
    cell_inc(p3, ((uint32_t const*)key) + 1);
    cell_inc(p4, ((uint32_t const*)key) + 1);

    return 1;
}

inline void twolevel_delete(TwoLevelPtr ptr) 
{
    rte_free(ptr);
}

inline uint32_t twolevel_num_searches(TwoLevelPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_twolevel_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchTwoLevelPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchTwoLevel), 64, socket); 

    module->_m.execute = simdbatch_twolevel_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->twolevel_ptr1 = twolevel_create(size, keysize, elsize, socket);
    module->twolevel_ptr2 = twolevel_create(size, keysize, elsize, socket);

    module->twolevel = module->twolevel_ptr1;

    return (ModulePtr)module;
}

void simdbatch_twolevel_delete(ModulePtr module_) 
{
    ModuleSimdBatchTwoLevelPtr module = (ModuleSimdBatchTwoLevelPtr)module_;

    twolevel_delete(module->twolevel_ptr1);
    twolevel_delete(module->twolevel_ptr2);

    rte_free(module);
}

inline void simdbatch_twolevel_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchTwoLevelPtr module = (ModuleSimdBatchTwoLevelPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    TwoLevelPtr twolevel = module->twolevel;

    /* Prefetch twolevel entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        twolevel_inc(twolevel, (pkt + 26));
    }
}

inline void simdbatch_twolevel_reset(ModulePtr module_) 
{
    ModuleSimdBatchTwoLevelPtr module = (ModuleSimdBatchTwoLevelPtr)module_;
    TwoLevelPtr prev = module->twolevel;

    if (module->twolevel == module->twolevel_ptr1) 
    {
        module->twolevel = module->twolevel_ptr2;
    } 
    else 
    {
        module->twolevel = module->twolevel_ptr1;
    }

    module->stats_search += twolevel_num_searches(prev);
    twolevel_reset(prev);
}

inline void simdbatch_twolevel_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchTwoLevelPtr module = (ModuleSimdBatchTwoLevelPtr)module_;
    module->stats_search += twolevel_num_searches(module->twolevel);
    fprintf(f, "SimdBatch::TwoLevel::SearchLoad\t%u\n", module->stats_search);
}


