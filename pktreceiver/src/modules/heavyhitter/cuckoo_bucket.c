#include <stdio.h>
#include <stdlib.h>

#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"

#include "../../common.h"
#include "../../experiment.h"
#include "../../vendor/murmur3.h"
#include "../../module.h"
#include "../../net.h"
#include "../../reporter.h"

#include "../../dss/hashmap_cuckoo_bucket.h"

#include "common.h"
#include "cuckoo_bucket.h"

ModulePtr heavyhitter_cuckoo_bucket_init(ModuleConfigPtr conf) {
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");

    ReporterPtr reporter = reporter_init(
            mc_uint32_get(conf, "reporter-size"), 
            keysize * 4, socket,
            mc_string_get(conf, "file-prefix"));

    ModuleHeavyHitterCuckooBPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleHeavyHitterCuckooB), 64, socket); 

    module->_m.execute = heavyhitter_cuckoo_bucket_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;
    module->reporter = reporter;

    module->hashmap_ptr1 = hashmap_cuckoo_bucket_create(size, keysize, 1, socket);
    module->hashmap_ptr2 = hashmap_cuckoo_bucket_create(size, keysize, 1, socket);

    module->hashmap = module->hashmap_ptr1;
    module->vals1   = rte_zmalloc_socket(0, sizeof(uint32_t)*elsize*size, 64, socket);
    module->vals2   = rte_zmalloc_socket(0, sizeof(uint32_t)*elsize*size, 64, socket);
    module->values  = module->vals1;

    module->idx1 = 0;
    module->idx2 = 0;
    module->index = &module->idx1;


    return (ModulePtr)module;
}

void heavyhitter_cuckoo_bucket_delete(ModulePtr module_) {
    ModuleHeavyHitterCuckooBPtr module = (ModuleHeavyHitterCuckooBPtr)module_;

    hashmap_cuckoo_bucket_delete(module->hashmap_ptr1);
    hashmap_cuckoo_bucket_delete(module->hashmap_ptr2);

    rte_free(module->vals1);
    rte_free(module->vals2);
    rte_free(module);
}

inline static void *
heavyhitter_get_value(ModuleHeavyHitterCuckooBPtr module, uint32_t idx) {
    return (module->values + (sizeof(uint32_t) * (idx-module->elsize)));
}

inline void
heavyhitter_cuckoo_bucket_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) {
    (void)(port);

    ModuleHeavyHitterCuckooBPtr module = (ModuleHeavyHitterCuckooBPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); (void)(timer);
    void *ptrs[MAX_PKT_BURST];
    HashMapCuckooBucketPtr hashmap = module->hashmap;
    unsigned elsize = module->elsize;

    /* Prefetch hashmap entries */
    for (i = 0; i < count; ++i) {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        void *ptr = hashmap_cuckoo_bucket_get_copy_key(hashmap, (pkt + 26));

        ptrs[i] = ptr;
        rte_prefetch0(ptr);
    }

    /* Save and report if necessary */
    ReporterPtr reporter = module->reporter;
    for (i = 0; i < count; ++i) { 
        uint32_t *ptr = (uint32_t*)ptrs[i];

        /* Index hasn't been assigned */
        if (*ptr == 0) { *module->index += elsize; *ptr = *module->index; }

        uint32_t *bc = heavyhitter_get_value(module, *ptr);
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);

        if (heavyhitter_copy_and_inc(bc, pkt, elsize)) {
            reporter_add_entry(reporter, pkt+26);
        }
    }
}

inline void
heavyhitter_cuckoo_bucket_reset(ModulePtr module_) {
    ModuleHeavyHitterCuckooBPtr module = (ModuleHeavyHitterCuckooBPtr)module_;
    HashMapCuckooBucketPtr prev = module->hashmap;
    volatile uint32_t *prev_idx = module->index;
    uint8_t *values = module->values;

    reporter_tick(module->reporter);

    if (module->hashmap == module->hashmap_ptr1) {
        module->hashmap = module->hashmap_ptr2;
        module->values  = module->vals2;
        module->index   = &module->idx2;
    } else {
        module->hashmap = module->hashmap_ptr1;
        module->values  = module->vals1;
        module->index   = &module->idx1;
    }

    module->stats_search += hashmap_cuckoo_bucket_num_searches(prev);

    hashmap_cuckoo_bucket_reset(prev);
    *prev_idx = 0;
    memset(values, 0, sizeof(uint32_t) * module->elsize * module->size);
}

inline void
heavyhitter_cuckoo_bucket_stats(ModulePtr module_, FILE *f) {
    ModuleHeavyHitterCuckooBPtr module = (ModuleHeavyHitterCuckooBPtr)module_;
    module->stats_search += hashmap_cuckoo_bucket_num_searches(module->hashmap);
    fprintf(f, "HeavyHitter::CuckooBucket::SearchLoad\t%u\n", module->stats_search);
}



