#include "rte_atomic.h"
#include "rte_memcpy.h"
#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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


#ifdef REPORT

    FILE * revsketch_res = fopen("revsketch_res.txt", "a");
    // fprintf(log_res, "pktid = %d\n", ptr->epoch_id);
    fprintf(revsketch_res, "----------------------Report--------------------\n");

    HashMapLinearPtr ht = ptr->ht;

    uint32_t ht_size = get_size(ht);
    uint32_t *table = get_table(ht);

    uint32_t hh_num = 0;
    for(uint32_t i = 0; i < ht_size; i++)
    {
        uint64_t key = *(uint64_t*)(table + i * 3);
        uint32_t value = *(uint32_t*)(table + i * 3 + 2);

        if(key != 0 && value == 0x12345688)
            hh_num++;
    }
    fprintf(revsketch_res, "%u;", hh_num);

    for(uint32_t i = 0; i < ht_size; i++)
    {
        uint64_t key = *(uint64_t*)(table + i * 3);
        uint32_t value = *(uint32_t*)(table + i * 3 + 2);
        if(key != 0 && value == 0x12345688)
            fprintf(revsketch_res, "%llu;", (long long unsigned int)key);
    }


    fprintf(revsketch_res, "\n");

    fflush(revsketch_res);
    fclose(revsketch_res);
#endif

    hashmap_linear_reset(ptr->ht);
    memset(ptr->table, 0, 2 * ptr->size * sizeof(uint32_t));
}

RevSketchPtr revsketch_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);

    RevSketchPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct RevSketch) + 
        2 * size * sizeof(uint32_t), /* Size of elements */
        64, socket);

    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize; // we don't save the keys

    ptr->row1 = ptr->table;
    // ptr->row2 = ptr->table + (size * elsize);
    // ptr->row3 = ptr->table + (size * 2 * elsize);


    ptr->row1_last = ptr->table + (size * elsize);
    // ptr->row2_last = ptr->table + (size * 4 * elsize);
    // ptr->row3_last = ptr->table + (size * 5 * elsize);

    printf("size = %lf\n", 2 * size * 4.0 / (1024 * 1024));

    // each epoch has 0.14M flows, we use 0.2M entry in the hash table. 
    ptr->ht = hashmap_linear_create(200000, 2, 1, 0);

    return ptr;
}


// whether key exists or not?
inline int query_hash_rev(RevSketchPtr ptr, uint64_t key)
{
    uint32_t *find_res = hashmap_linear_get_nocopy_key(ptr->ht, &key);
    if(*(uint64_t*)(find_res - 2) == 0 || (*find_res) == (uint32_t)(1 << 31))
    {
        return -1;
    }
    return (*find_res);
}
inline void insert_hash_rev(RevSketchPtr ptr, uint64_t key, uint32_t value)
{
    uint32_t* find_res = hashmap_linear_get_copy_key(ptr->ht, &key);
    (*find_res) = value;
}
// only set the value part to (1 << 31);
inline void delete_hash_rev(RevSketchPtr ptr, uint64_t key)
{
    uint32_t *find_res = hashmap_linear_get_nocopy_key(ptr->ht, &key);
    // uint32_t *find_res = hashmap_linear_get_copy_key(ptr->ht, &key);

    // find it;
    if(*(uint64_t*)(find_res - 2) == key)
    {
        (*find_res) = (1 << 31);
    }
}


// inline int get_min_value(uint32_t * p1, uint32_t * p2, uint32_t *p3)
// {
//     uint32_t tmin = 1 << 30;
//     tmin = (*p1) < tmin ? (*p1) : tmin;
//     // tmin = (*p2) < tmin ? (*p2) : tmin;
//     // tmin = (*p3) < tmin ? (*p3) : tmin;
//     return tmin;
// }

inline int revsketch_inc(RevSketchPtr ptr, void const *key) 
{
    // uint8_t const * p_seq = *(uint8_t const *)key;
    // p_seq += 12
    // ptr->epoch_id = (*(uint32_t*)p_seq) / (2 * 1024 * 1024);


    rte_atomic32_inc(&ptr->stats_search);

    uint32_t size = ptr->size;

    uint32_t h1 = dss_hash_with_seed(key, 2, CMS_SH1) & (size - 1);
    uint32_t *p1 = ptr->row1 + h1;
    uint32_t *p1_last = ptr->row1_last + h1;
    rte_prefetch0(p1);
    rte_prefetch0(p1_last);


    // uint32_t h2 = dss_hash_with_seed(key, 2, CMS_SH2 + CMS_SH1) & (size - 1);
    // uint32_t *p2 = ptr->row2 + h2;
    // uint32_t *p2_last = ptr->row2_last + h2;
    // rte_prefetch0(p2);
    // rte_prefetch0(p2_last);


    // uint32_t h3 = dss_hash_with_seed(key, 2, CMS_SH3 + CMS_SH2) & (size - 1);
    // uint32_t *p3 = ptr->row3 + h3;
    // uint32_t *p3_last = ptr->row3_last + h3;
    // rte_prefetch0(p3);
    // rte_prefetch0(p3_last);

    (*p1)++;
    // (*p2)++;
    // (*p3)++;


    int value_now = (*p1);
    int value_last = (*p1_last);

    int delta_value = value_now - value_last;
    delta_value = delta_value >= 0 ? delta_value : -delta_value;
    
    // printf("%d %lu ", value_now, *(uint64_t*)key);
    // printf("%d vs. %d\n", value_now, value_last);

    if(delta_value > 1000)
    {
        insert_hash_rev(ptr, *(uint64_t const*)key, 0x12345688);
    }
    else
    {
        delete_hash_rev(ptr, *(uint64_t const*)key);
    }

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







ModulePtr heavychange_revsketch_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleHeavyChangeRevSketchPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleHeavyChangeRevSketch), 64, socket); 

    module->_m.execute = heavychange_revsketch_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->revsketch_ptr1 = revsketch_create(size, keysize, elsize, socket);
    module->revsketch_ptr2 = revsketch_create(size, keysize, elsize, socket);

    module->revsketch = module->revsketch_ptr1;

    return (ModulePtr)module;
}

void heavychange_revsketch_delete(ModulePtr module_) 
{
    ModuleHeavyChangeRevSketchPtr module = (ModuleHeavyChangeRevSketchPtr)module_;

    revsketch_delete(module->revsketch_ptr1);
    revsketch_delete(module->revsketch_ptr2);

    rte_free(module);
}

inline void heavychange_revsketch_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleHeavyChangeRevSketchPtr module = (ModuleHeavyChangeRevSketchPtr)module_;
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

inline void heavychange_revsketch_reset(ModulePtr module_) 
{
    ModuleHeavyChangeRevSketchPtr module = (ModuleHeavyChangeRevSketchPtr)module_;
    RevSketchPtr prev = module->revsketch;
    uint32_t cmsize = prev->size * 1 * sizeof(uint32_t);

    if (module->revsketch == module->revsketch_ptr1) 
    {
        rte_memcpy(module->revsketch_ptr2->row1_last, module->revsketch_ptr1->row1, cmsize);
        module->revsketch = module->revsketch_ptr2;
    } 
    else 
    {
        rte_memcpy(module->revsketch_ptr1->row1_last, module->revsketch_ptr2->row1, cmsize);
        module->revsketch = module->revsketch_ptr1;
    }

    module->stats_search += revsketch_num_searches(prev);
    revsketch_reset(prev);
}

inline void heavychange_revsketch_stats(ModulePtr module_, FILE *f) 
{
    ModuleHeavyChangeRevSketchPtr module = (ModuleHeavyChangeRevSketchPtr)module_;
    module->stats_search += revsketch_num_searches(module->revsketch);
    fprintf(f, "HeavyChange::RevSketch::SearchLoad\t%u\n", module->stats_search);
}


