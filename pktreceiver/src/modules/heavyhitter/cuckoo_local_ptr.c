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

#include "../../dss/hashmap_cuckoo.h"

#include "common.h"
#include "cuckoo_local_ptr.h"

ModulePtr heavyhitter_cuckoo_local_ptr_init(ModuleConfigPtr conf) {
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");

    printf("Here: %d, %d, %d, %d\n", size, keysize, elsize, socket);

    ReporterPtr reporter = reporter_init(
            mc_uint32_get(conf, "reporter-size"), 
            keysize * 4, socket,
            mc_string_get(conf, "file-prefix"));

    ModuleHeavyHitterCuckooLPPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleHeavyHitterCuckooLP), 64, socket); 

    module->_m.execute = heavyhitter_cuckoo_local_ptr_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;
    module->reporter = reporter;
    module->index = 0;

    module->hashmap_ptr1 = hashmap_cuckoo_create(size, keysize, sizeof(uintptr_t)/sizeof(uint32_t), socket);
    module->hashmap_ptr2 = hashmap_cuckoo_create(size, keysize, sizeof(uintptr_t)/sizeof(uint32_t), socket);

    module->hashmap = module->hashmap_ptr1;
    module->vals1 = rte_zmalloc_socket(0,
            sizeof(uint32_t) * module->elsize * module->size, 64, socket);
    module->vals2 = rte_zmalloc_socket(0,
            sizeof(uint32_t) * module->elsize * module->size, 64, socket);

    module->values = module->vals1;
    module->index = 0;

    return (ModulePtr)module;
}

void heavyhitter_cuckoo_local_ptr_delete(ModulePtr module_) {
    ModuleHeavyHitterCuckooLPPtr module = (ModuleHeavyHitterCuckooLPPtr)module_;

    hashmap_cuckoo_delete(module->hashmap_ptr1);
    hashmap_cuckoo_delete(module->hashmap_ptr2);

    rte_free(module);
}

#define get_ipv4(mbuf) ((struct ipv4_hdr const*) \
        ((rte_pktmbuf_mtod(pkts[i], uint8_t const*)) + \
        (sizeof(struct ether_addr))))

inline void
heavyhitter_cuckoo_local_ptr_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) {
    (void)(port);

    ModuleHeavyHitterCuckooLPPtr module = (ModuleHeavyHitterCuckooLPPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); (void)(timer);
    void *ptrs[MAX_PKT_BURST];
    HashMapCuckooPtr hashmap = module->hashmap;
    unsigned elsize = module->elsize;

    /* Prefetch hashmap entries */
    for (i = 0; i < count; ++i) {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        void *ptr = hashmap_cuckoo_get_copy_key(hashmap, (pkt + 26));

        ptrs[i] = ptr;
        rte_prefetch0(ptr);
    }

    /* Save and report if necessary */
    ReporterPtr reporter = module->reporter;
    for (i = 0; i < count; ++i) { 
        uintptr_t *ptr = ptrs[i];
        if (*ptr == 0) {
            *ptr = (uintptr_t)(module->values + 4 * module->elsize * module->index);
            module->index++;
        }

        uint32_t *bc = (uint32_t*)(*ptr); (*bc)++;
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);

        if (heavyhitter_copy_and_inc(bc, pkt, elsize)) {
            reporter_add_entry(reporter, pkt+26);
        }
    }
}

inline void
heavyhitter_cuckoo_local_ptr_reset(ModulePtr module_) {
    ModuleHeavyHitterCuckooLPPtr module = (ModuleHeavyHitterCuckooLPPtr)module_;
    HashMapCuckooPtr prev = module->hashmap;
    uint8_t *prev_val = module->values;
    reporter_tick(module->reporter);

    module->index = 0;
    if (module->hashmap == module->hashmap_ptr1) {
        module->hashmap = module->hashmap_ptr2;
        module->values = module->vals2;
    } else {
        module->hashmap = module->hashmap_ptr1;
        module->values = module->vals1;
    }

    module->stats_search += hashmap_cuckoo_num_searches(prev);
    memset(prev_val, 0, sizeof(uint32_t)*module->elsize*module->size);
    hashmap_cuckoo_reset(prev);
}

inline void
heavyhitter_cuckoo_local_ptr_stats(ModulePtr module_, FILE *f) {
    ModuleHeavyHitterCuckooLPPtr module = (ModuleHeavyHitterCuckooLPPtr)module_;
    module->stats_search += hashmap_cuckoo_num_searches(module->hashmap);
    fprintf(f, "HeavyHitter::CuckooPtr::SearchLoad\t%u\n", module->stats_search);
}



