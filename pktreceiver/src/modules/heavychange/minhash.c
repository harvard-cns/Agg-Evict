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

#include "minhash.h"



inline void minhash_reset(MinHashPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);

#ifdef REPORT

    FILE * minhash_res = fopen("minhash_res.txt", "a");
    fprintf(minhash_res, "----------------------Report--------------------\n");

    hashmap_report(ptr->ht_fs, minhash_res);

    fprintf(minhash_res, "\n");

    fflush(minhash_res);
    fclose(minhash_res);
#endif

    hashmap_reset(ptr->ht_fs);
}

MinHashPtr minhash_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    MinHashPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct MinHash), /* Size of elements */
        64, socket);


    // for (h & ptr->size) to function as %;     
    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->ht_fs = hashmap_create(size, keysize, elsize, socket);

    printf("size = %lf\n", size * 12.0 / (1024 * 1024));
    return ptr;
}



inline int minhash_inc(MinHashPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);

    // find a key. 
    uint32_t *find_res;
    find_res = hashmap_get_copy_key(ptr->ht_fs, key);
    (*find_res) ++;

    return 1;
}

inline void minhash_delete(MinHashPtr ptr) 
{
    rte_free(ptr);
}

inline uint32_t minhash_num_searches(MinHashPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr heavychange_minhash_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleHeavyChangeMinHashPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleHeavyChangeMinHash), 64, socket); 

    module->_m.execute = heavychange_minhash_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->minhash_ptr1 = minhash_create(size, keysize, elsize, socket);
    module->minhash_ptr2 = minhash_create(size, keysize, elsize, socket);

    module->minhash = module->minhash_ptr1;

    return (ModulePtr)module;
}

void heavychange_minhash_delete(ModulePtr module_) 
{
    ModuleHeavyChangeMinHashPtr module = (ModuleHeavyChangeMinHashPtr)module_;

    minhash_delete(module->minhash_ptr1);
    minhash_delete(module->minhash_ptr2);

    rte_free(module);
}

inline void heavychange_minhash_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleHeavyChangeMinHashPtr module = (ModuleHeavyChangeMinHashPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    MinHashPtr minhash = module->minhash;

    /* Prefetch minhash entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        minhash_inc(minhash, (pkt + 26));
    }
}

inline void heavychange_minhash_reset(ModulePtr module_) 
{
    ModuleHeavyChangeMinHashPtr module = (ModuleHeavyChangeMinHashPtr)module_;
    MinHashPtr prev = module->minhash;

    if (module->minhash == module->minhash_ptr1) 
    {
        module->minhash = module->minhash_ptr2;
    } 
    else 
    {
        module->minhash = module->minhash_ptr1;
    }

    module->stats_search += minhash_num_searches(prev);
    minhash_reset(prev);
}

inline void heavychange_minhash_stats(ModulePtr module_, FILE *f) 
{
    ModuleHeavyChangeMinHashPtr module = (ModuleHeavyChangeMinHashPtr)module_;
    module->stats_search += minhash_num_searches(module->minhash);
    fprintf(f, "HeavyChange::MinHash::SearchLoad\t%u\n", module->stats_search);
}


