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

#include "pkttest.h"




inline void pkttest_reset(PktTestPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);
#ifdef REPORT
    FILE * pkttest_res = fopen("pkttest_res.txt", "a");
    // fprintf(pkttest_res, "pktid = %d\n", ptr->epoch_id);
    fprintf(pkttest_res, "----------------------Report--------------------\n");
    uint64_t * p_key = ptr->row_key;
    uint32_t cur_size = ptr->cur_size;
    

    fprintf(pkttest_res, "%u;", cur_size);
    for(uint32_t i = 0; i < cur_size; i++)
    {
        uint32_t * p = (uint32_t *)&p_key[i];
        fprintf(pkttest_res, "%u,%u;", *p, *(p+1));

        // fprintf(pkttest_res, "%lu;", p_key[i]);
    }
    fprintf(pkttest_res, "\n");

    fflush(pkttest_res);
    fclose(pkttest_res);
#endif

    memset(ptr->table, 0, ptr->size * (ptr->keysize + 1) * sizeof(uint32_t));
    ptr->cur_size = 0;
}

PktTestPtr pkttest_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    PktTestPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct PktTest) + 
        (size) * (keysize + 1) * sizeof(uint32_t), /* Size of elements */
        64, socket);


    // for (h & ptr->size) to function as %;     
    ptr->size = size - 1;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys
    ptr->cur_size = 0;

    ptr->row_key = (uint64_t*)ptr->table;

    return ptr;
}




inline int pkttest_inc(PktTestPtr ptr, void const *key) 
{
    // uint8_t const * p_seq = (uint8_t const *)key;
    // p_seq += 12;
    // ptr->epoch_id = (*(uint32_t const*)p_seq);

    rte_atomic32_inc(&ptr->stats_search);

    uint32_t cur_size = ptr->cur_size;
    uint64_t * p_key = ptr->row_key;
    p_key[cur_size] = *(uint64_t const*)key;
    ptr->cur_size ++;

    return 1;
}

inline void pkttest_delete(PktTestPtr ptr) 
{
    rte_free(ptr);
}

inline uint32_t pkttest_num_searches(PktTestPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr heavychange_pkttest_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleHeavyChangePktTestPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleHeavyChangePktTest), 64, socket); 

    module->_m.execute = heavychange_pkttest_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->pkttest_ptr1 = pkttest_create(size, keysize, elsize, socket);
    module->pkttest_ptr2 = pkttest_create(size, keysize, elsize, socket);

    module->pkttest = module->pkttest_ptr1;

    return (ModulePtr)module;
}

void heavychange_pkttest_delete(ModulePtr module_) 
{
    ModuleHeavyChangePktTestPtr module = (ModuleHeavyChangePktTestPtr)module_;

    pkttest_delete(module->pkttest_ptr1);
    pkttest_delete(module->pkttest_ptr2);

    rte_free(module);
}

inline void heavychange_pkttest_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleHeavyChangePktTestPtr module = (ModuleHeavyChangePktTestPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    PktTestPtr pkttest = module->pkttest;

    /* Prefetch pkttest entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        pkttest_inc(pkttest, (pkt + 26));
    }
}

inline void heavychange_pkttest_reset(ModulePtr module_) 
{
    ModuleHeavyChangePktTestPtr module = (ModuleHeavyChangePktTestPtr)module_;
    PktTestPtr prev = module->pkttest;

    if (module->pkttest == module->pkttest_ptr1) 
    {
        module->pkttest = module->pkttest_ptr2;
    } 
    else 
    {
        module->pkttest = module->pkttest_ptr1;
    }

    module->stats_search += pkttest_num_searches(prev);
    pkttest_reset(prev);
}

inline void heavychange_pkttest_stats(ModulePtr module_, FILE *f) 
{
    ModuleHeavyChangePktTestPtr module = (ModuleHeavyChangePktTestPtr)module_;
    module->stats_search += pkttest_num_searches(module->pkttest);
    fprintf(f, "HeavyChange::PktTest::SearchLoad\t%u\n", module->stats_search);
}


