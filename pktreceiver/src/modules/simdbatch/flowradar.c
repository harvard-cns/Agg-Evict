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

#include "flowradar.h"




inline static uint32_t flowradar_size(FlowRadarPtr ptr) 
{
    return ptr->size * (sizeof(struct Cell) + 4);
}

inline void flowradar_reset(FlowRadarPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
    memset(ptr->table, 0, flowradar_size(ptr));
}

FlowRadarPtr flowradar_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);
    // the number of cells in iblt of FR: size;
    // the number of bits in bf of FR: size * 8;


    FlowRadarPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct FlowRadar) + 
        (size) * (sizeof(struct Cell) + 4), /* Size of elements */
        64, socket);

    printf("%lf\n", ((size) * (sizeof(struct Cell) + 4)) * 1.0 / 1024 / 1024);

    // not size - 1 here!!!
    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize;

    ptr->rowbf = ptr->table;
    ptr->rowcell = (CellPtr)(ptr->table + (size));

    return ptr;
}




inline int flowradar_inc(FlowRadarPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

    uint32_t bfsize = ((ptr->size) << 5); // 32 * size, bit;
    uint32_t ibltsize = ptr->size; // cells;


    uint32_t h1 = dss_hash_with_seed(key, ptr->keysize, CMS_SH1) & (bfsize - 1);
    uint32_t *p1 = ptr->rowbf + (h1 >> 5);
    rte_prefetch0(p1);

    uint32_t h2 = dss_hash_with_seed(key, ptr->keysize, CMS_SH2 + CMS_SH1) & (bfsize - 1);
    uint32_t *p2 = ptr->rowbf + (h2 >> 5);
    rte_prefetch0(p2);

    uint32_t h3 = dss_hash_with_seed(key, ptr->keysize, CMS_SH3 + CMS_SH2) & (bfsize - 1);
    uint32_t *p3 = ptr->rowbf + (h3 >> 5);
    rte_prefetch0(p3);

    uint32_t h4 = dss_hash_with_seed(key, ptr->keysize, CMS_SH4 + CMS_SH3) & (bfsize - 1);
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




    h1 = dss_hash_with_seed(key, ptr->keysize, CMS_SH5) & (ibltsize - 1);
    CellPtr p1_cell = ptr->rowcell + h1;
    rte_prefetch0(p1_cell);

    h2 = dss_hash_with_seed(key, ptr->keysize, CMS_SH6 + CMS_SH5) & (ibltsize - 1);
    CellPtr p2_cell = ptr->rowcell + h2;
    rte_prefetch0(p2_cell);

    h3 = dss_hash_with_seed(key, ptr->keysize, CMS_SH7 + CMS_SH5) & (ibltsize - 1);
    CellPtr p3_cell = ptr->rowcell + h3;
    rte_prefetch0(p3_cell);

    h4 = dss_hash_with_seed(key, ptr->keysize, CMS_SH8 + CMS_SH5) & (ibltsize - 1);
    CellPtr p4_cell = ptr->rowcell + h4;
    rte_prefetch0(p4_cell);


    // a new flow!
    if(flag == 0)
    {
        p1_cell->flowx ^= *(uint32_t const*)key;
        p1_cell->flowc ++;

        p2_cell->flowx ^= *(uint32_t const*)key;
        p2_cell->flowc ++;
        
        p3_cell->flowx ^= *(uint32_t const*)key;
        p3_cell->flowc ++;
        
        p4_cell->flowx ^= *(uint32_t const*)key;
        p4_cell->flowc ++;
    }

    p1_cell->packetc ++;
    p2_cell->packetc ++;
    p3_cell->packetc ++;
    p4_cell->packetc ++;

    return 1;
}

inline void flowradar_delete(FlowRadarPtr ptr) 
{
    rte_free(ptr);
}

inline uint32_t flowradar_num_searches(FlowRadarPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_flowradar_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchFlowRadarPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchFlowRadar), 64, socket); 

    module->_m.execute = simdbatch_flowradar_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->flowradar_ptr1 = flowradar_create(size, keysize, elsize, socket);
    module->flowradar_ptr2 = flowradar_create(size, keysize, elsize, socket);

    module->flowradar = module->flowradar_ptr1;

    return (ModulePtr)module;
}

void simdbatch_flowradar_delete(ModulePtr module_) 
{
    ModuleSimdBatchFlowRadarPtr module = (ModuleSimdBatchFlowRadarPtr)module_;

    flowradar_delete(module->flowradar_ptr1);
    flowradar_delete(module->flowradar_ptr2);

    rte_free(module);
}

inline void simdbatch_flowradar_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchFlowRadarPtr module = (ModuleSimdBatchFlowRadarPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    FlowRadarPtr flowradar = module->flowradar;

    /* Prefetch flowradar entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        flowradar_inc(flowradar, (pkt + 26));
    }
}

inline void simdbatch_flowradar_reset(ModulePtr module_) 
{
    ModuleSimdBatchFlowRadarPtr module = (ModuleSimdBatchFlowRadarPtr)module_;
    FlowRadarPtr prev = module->flowradar;

    if (module->flowradar == module->flowradar_ptr1) 
    {
        module->flowradar = module->flowradar_ptr2;
    } 
    else 
    {
        module->flowradar = module->flowradar_ptr1;
    }

    module->stats_search += flowradar_num_searches(prev);
    flowradar_reset(prev);
}

inline void simdbatch_flowradar_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchFlowRadarPtr module = (ModuleSimdBatchFlowRadarPtr)module_;
    module->stats_search += flowradar_num_searches(module->flowradar);
    fprintf(f, "SimdBatch::FlowRadar::SearchLoad\t%u\n", module->stats_search);
}


