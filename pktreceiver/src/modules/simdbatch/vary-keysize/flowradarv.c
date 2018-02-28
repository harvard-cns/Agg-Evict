#include "rte_atomic.h"
#include "rte_memcpy.h"
#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"


#include <stdio.h>
#include <stdlib.h>


#include "../../../common.h"
#include "../../../experiment.h"
#include "../../../module.h"
#include "../../../net.h"
#include "../../../vendor/murmur3.h"
#include "../../../dss/hash.h"

#include "../../heavyhitter/common.h"

#include "flowradarv.h"




inline static uint32_t flowradarv_size(FlowRadarVPtr ptr) 
{
    return ptr->size * (sizeof(struct CellV) + 4);
}

inline void flowradarv_reset(FlowRadarVPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
    memset(ptr->table, 0, flowradarv_size(ptr));
}

FlowRadarVPtr flowradarv_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);
    // the number of cells in iblt of FR: size;
    // the number of bits in bf of FR: size * 32;


    FlowRadarVPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct FlowRadarV) + 
        (size) * (sizeof(struct CellV) + 4), /* Size of elements */
        64, socket);

    // not size - 1 here!!!
    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize;

    ptr->rowbf = ptr->table;
    ptr->rowcell = (CellVPtr)(ptr->table + (size));

    return ptr;
}




inline int flowradarv_inc(FlowRadarVPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

    uint32_t bfsize = ((ptr->size) << 5); // 32 * size, bit;
    uint32_t ibltsize = ptr->size; // cells;

    uint32_t mykey[4];
    uint32_t mykeysize;

#ifdef SIP
    mykey[0] = *(uint32_t const *)(key);
    mykeysize = 1;
#endif 

#ifdef DIP
    mykey[0] = *((uint32_t const *)(key) + 1);
    mykeysize = 1;
#endif 

#ifdef SD_PAIR
    mykey[0] = *(uint32_t const *)(key);
    mykey[1] = *((uint32_t const *)(key) + 1);
    mykeysize = 2;
#endif 

#ifdef FOUR_TUPLE
    mykey[0] = *(uint32_t const *)(key);
    mykey[1] = *((uint32_t const *)(key) + 1);
    mykey[2] = *((uint32_t const *)(key) + 2);
    mykeysize = 3;
#endif 


#ifdef FIVE_TUPLE
    mykey[0] = *(uint32_t const *)(key);
    mykey[1] = *((uint32_t const *)(key) + 1);
    mykey[2] = *((uint32_t const *)(key) + 2);
    mykey[3] = *((uint8_t const *)(key) - 3);
    mykeysize = 4;
#endif 



    uint32_t h1 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH1) & (bfsize - 1);
    uint32_t *p1 = ptr->rowbf + (h1 >> 5);
    rte_prefetch0(p1);

    uint32_t h2 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH2 + CMS_SH1) & (bfsize - 1);
    uint32_t *p2 = ptr->rowbf + (h2 >> 5);
    rte_prefetch0(p2);

    uint32_t h3 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH3 + CMS_SH2) & (bfsize - 1);
    uint32_t *p3 = ptr->rowbf + (h3 >> 5);
    rte_prefetch0(p3);

    uint32_t h4 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH4 + CMS_SH3) & (bfsize - 1);
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




    h1 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH5) & (ibltsize - 1);
    CellVPtr p1_cell = ptr->rowcell + h1;
    rte_prefetch0(p1_cell);

    h2 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH6 + CMS_SH5) & (ibltsize - 1);
    CellVPtr p2_cell = ptr->rowcell + h2;
    rte_prefetch0(p2_cell);

    h3 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH7 + CMS_SH5) & (ibltsize - 1);
    CellVPtr p3_cell = ptr->rowcell + h3;
    rte_prefetch0(p3_cell);

    h4 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH8 + CMS_SH5) & (ibltsize - 1);
    CellVPtr p4_cell = ptr->rowcell + h4;
    rte_prefetch0(p4_cell);


    // a new flow!
    if(flag == 0)
    {
#ifdef SIP
        p1_cell->sip ^= mykey[0];
        p2_cell->sip ^= mykey[0];
        p3_cell->sip ^= mykey[0];
        p4_cell->sip ^= mykey[0];
#endif

#ifdef DIP
        p1_cell->dip ^= mykey[0];
        p2_cell->dip ^= mykey[0];
        p3_cell->dip ^= mykey[0];
        p4_cell->dip ^= mykey[0];
#endif

#ifdef SD_PAIR
        p1_cell->sip ^= mykey[0];
        p1_cell->dip ^= mykey[1];

        p2_cell->sip ^= mykey[0];
        p2_cell->dip ^= mykey[1];

        p3_cell->sip ^= mykey[0];
        p3_cell->dip ^= mykey[1];
        
        p4_cell->sip ^= mykey[0];
        p4_cell->dip ^= mykey[1];
#endif

#ifdef FOUR_TUPLE
        p1_cell->sip ^= mykey[0];
        p1_cell->dip ^= mykey[1];
        p1_cell->port ^= mykey[2];

        p2_cell->sip ^= mykey[0];
        p2_cell->dip ^= mykey[1];
        p2_cell->port ^= mykey[2];

        p3_cell->sip ^= mykey[0];
        p3_cell->dip ^= mykey[1];
        p3_cell->port ^= mykey[2];

        p4_cell->sip ^= mykey[0];
        p4_cell->dip ^= mykey[1];
        p4_cell->port ^= mykey[2];
#endif

#ifdef FIVE_TUPLE
        p1_cell->sip ^= mykey[0];
        p1_cell->dip ^= mykey[1];
        p1_cell->port ^= mykey[2];
        p1_cell->type ^= mykey[3];

        p2_cell->sip ^= mykey[0];
        p2_cell->dip ^= mykey[1];
        p2_cell->port ^= mykey[2];
        p2_cell->type ^= mykey[3];

        p3_cell->sip ^= mykey[0];
        p3_cell->dip ^= mykey[1];
        p3_cell->port ^= mykey[2];
        p3_cell->type ^= mykey[3];
        
        p4_cell->sip ^= mykey[0];
        p4_cell->dip ^= mykey[1];
        p4_cell->port ^= mykey[2];
        p4_cell->type ^= mykey[3];
#endif
  
        p1_cell->flowc ++;
        p2_cell->flowc ++;
        p3_cell->flowc ++;
        p4_cell->flowc ++;
    }

    p1_cell->packetc ++;
    p2_cell->packetc ++;
    p3_cell->packetc ++;
    p4_cell->packetc ++;

    return 1;
}

inline void flowradarv_delete(FlowRadarVPtr ptr) 
{
    rte_free(ptr);
}

inline uint32_t flowradarv_num_searches(FlowRadarVPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_flowradarv_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchFlowRadarVPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchFlowRadarV), 64, socket); 

    module->_m.execute = simdbatch_flowradarv_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->flowradarv_ptr1 = flowradarv_create(size, keysize, elsize, socket);
    module->flowradarv_ptr2 = flowradarv_create(size, keysize, elsize, socket);

    module->flowradarv = module->flowradarv_ptr1;

    return (ModulePtr)module;
}

void simdbatch_flowradarv_delete(ModulePtr module_) 
{
    ModuleSimdBatchFlowRadarVPtr module = (ModuleSimdBatchFlowRadarVPtr)module_;

    flowradarv_delete(module->flowradarv_ptr1);
    flowradarv_delete(module->flowradarv_ptr2);

    rte_free(module);
}

inline void simdbatch_flowradarv_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchFlowRadarVPtr module = (ModuleSimdBatchFlowRadarVPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    FlowRadarVPtr flowradarv = module->flowradarv;

    /* Prefetch flowradarv entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        flowradarv_inc(flowradarv, (pkt + 26));
    }
}

inline void simdbatch_flowradarv_reset(ModulePtr module_) 
{
    ModuleSimdBatchFlowRadarVPtr module = (ModuleSimdBatchFlowRadarVPtr)module_;
    FlowRadarVPtr prev = module->flowradarv;

    if (module->flowradarv == module->flowradarv_ptr1) 
    {
        module->flowradarv = module->flowradarv_ptr2;
    } 
    else 
    {
        module->flowradarv = module->flowradarv_ptr1;
    }

    module->stats_search += flowradarv_num_searches(prev);
    flowradarv_reset(prev);
}

inline void simdbatch_flowradarv_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchFlowRadarVPtr module = (ModuleSimdBatchFlowRadarVPtr)module_;
    module->stats_search += flowradarv_num_searches(module->flowradarv);
    fprintf(f, "SimdBatch::FlowRadarV::SearchLoad\t%u\n", module->stats_search);
}


