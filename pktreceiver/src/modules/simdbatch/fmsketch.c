#include "rte_atomic.h"
#include "rte_memcpy.h"
#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"


#include <stdio.h>
#include <stdlib.h>
#include <bmiintrin.h>


#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "../../vendor/murmur3.h"
#include "../../dss/hash.h"

#include "../heavyhitter/common.h"

#include "fmsketch.h"





inline void fmsketch_reset(FMSketchPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);


    FILE *file_fm = fopen("file_fm.txt", "a");
    fprintf(file_fm ,"%u\n", (uint32_t)ptr->count1);
    fprintf(file_fm ,"%u\n", (uint32_t)ptr->count2);
    fprintf(file_fm ,"%u\n", (uint32_t)ptr->count3);
    fprintf(file_fm ,"%u\n", (uint32_t)ptr->count4);
    fprintf(file_fm, "--------------------------2M-------------------------\n");
    fflush(file_fm);
    fclose(file_fm);



    ptr->count1 = 0;
    ptr->count2 = 0;
    ptr->count3 = 0;
    ptr->count4 = 0;
}

FMSketchPtr fmsketch_create(uint16_t keysize, int socket) 
{
    FMSketchPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct FMSketch), // we do not have any table;
        64, socket);

    ptr->keysize = keysize;
    return ptr;
}




inline int fmsketch_inc(FMSketchPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

    uint32_t h1 = dss_hash_with_seed(key, ptr->keysize, CMS_SH1);
    uint32_t h2 = dss_hash_with_seed(key, ptr->keysize, CMS_SH2 + CMS_SH1);
    uint32_t h3 = dss_hash_with_seed(key, ptr->keysize, CMS_SH3 + CMS_SH2);
    uint32_t h4 = dss_hash_with_seed(key, ptr->keysize, CMS_SH4 + CMS_SH3);

    uint32_t hnum1 = (h1 == 0 ? 31 : _tzcnt_u32(h1));
    uint32_t hnum2 = (h2 == 0 ? 31 : _tzcnt_u32(h2));
    uint32_t hnum3 = (h3 == 0 ? 31 : _tzcnt_u32(h3));
    uint32_t hnum4 = (h4 == 0 ? 31 : _tzcnt_u32(h4));

    ptr->count1 |= (1 << hnum1);
    ptr->count2 |= (1 << hnum2);
    ptr->count3 |= (1 << hnum3);
    ptr->count4 |= (1 << hnum4);

    return 1;
}

inline void fmsketch_delete(FMSketchPtr ptr) 
{
    rte_free(ptr);
}

inline uint32_t fmsketch_num_searches(FMSketchPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_fmsketch_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchFMSketchPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchFMSketch), 64, socket); 

    module->_m.execute = simdbatch_fmsketch_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->fmsketch_ptr1 = fmsketch_create(keysize, socket);
    module->fmsketch_ptr2 = fmsketch_create(keysize, socket);

    module->fmsketch = module->fmsketch_ptr1;

    return (ModulePtr)module;
}

void simdbatch_fmsketch_delete(ModulePtr module_) 
{
    ModuleSimdBatchFMSketchPtr module = (ModuleSimdBatchFMSketchPtr)module_;

    fmsketch_delete(module->fmsketch_ptr1);
    fmsketch_delete(module->fmsketch_ptr2);

    rte_free(module);
}

inline void simdbatch_fmsketch_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchFMSketchPtr module = (ModuleSimdBatchFMSketchPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    FMSketchPtr fmsketch = module->fmsketch;

    /* Prefetch fmsketch entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        fmsketch_inc(fmsketch, (pkt + 26));     
    }
}

inline void simdbatch_fmsketch_reset(ModulePtr module_) 
{
    ModuleSimdBatchFMSketchPtr module = (ModuleSimdBatchFMSketchPtr)module_;
    FMSketchPtr prev = module->fmsketch;

    if (module->fmsketch == module->fmsketch_ptr1) 
    {
        module->fmsketch = module->fmsketch_ptr2;
    } 
    else 
    {
        module->fmsketch = module->fmsketch_ptr1;
    }

    module->stats_search += fmsketch_num_searches(prev);
    fmsketch_reset(prev);
}

inline void simdbatch_fmsketch_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchFMSketchPtr module = (ModuleSimdBatchFMSketchPtr)module_;
    module->stats_search += fmsketch_num_searches(module->fmsketch);
    fprintf(f, "SimdBatch::FMSketch::SearchLoad\t%u\n", module->stats_search);
}




