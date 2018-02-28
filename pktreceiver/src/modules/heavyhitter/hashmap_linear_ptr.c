#include <stdio.h>
#include <stdlib.h>

#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"

#include "../../common.h"
#include "../../vendor/murmur3.h"
#include "../../module.h"
#include "../../net.h"
#include "../../reporter.h"

#include "../../dss/hashmap_linear.h"

#include "common.h"
#include "hashmap_linear_ptr.h"

ModulePtr heavyhitter_hashmap_linear_ptr_init(ModuleConfigPtr conf) {
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");

    ReporterPtr reporter = reporter_init(
            mc_uint32_get(conf, "reporter-size"), 
            keysize * 4, socket,
            mc_string_get(conf, "file-prefix"));

    ModuleHeavyHitterHashmapLinearPPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleHeavyHitterHashmapLinearPtr), 64, socket); 

    module->_m.execute = heavyhitter_hashmap_linear_ptr_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;
    module->reporter = reporter;

    module->hashmap_linear_ptr1 = hashmap_linear_create(
            size, keysize, sizeof(uintptr_t)/sizeof(uint32_t), socket);
    module->hashmap_linear_ptr2 = hashmap_linear_create(
            size, keysize, sizeof(uintptr_t)/sizeof(uint32_t), socket);

    module->hashmap_linear = module->hashmap_linear_ptr1;

    module->vals1 = rte_zmalloc_socket(0,
            sizeof(uint32_t) * elsize * size, 64, socket);
    module->vals2 = rte_zmalloc_socket(0,
            sizeof(uint32_t) * elsize * size, 64, socket);

    module->index = 0;
    module->values = module->vals1;

    return (ModulePtr)module;
}

void heavyhitter_hashmap_linear_ptr_delete(ModulePtr module_) {
    ModuleHeavyHitterHashmapLinearPPtr module = (ModuleHeavyHitterHashmapLinearPPtr)module_;

    hashmap_linear_delete(module->hashmap_linear_ptr1);
    hashmap_linear_delete(module->hashmap_linear_ptr2);

    rte_free(module);
}

#define get_ipv4(mbuf) ((struct ipv4_hdr const*) \
        ((rte_pktmbuf_mtod(pkts[i], uint8_t const*)) + \
        (sizeof(struct ether_addr))))

inline void
heavyhitter_hashmap_linear_ptr_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) {
    (void)(port);

    ModuleHeavyHitterHashmapLinearPPtr module = (ModuleHeavyHitterHashmapLinearPPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); (void)(timer);
    void *ptrs[MAX_PKT_BURST];
    HashMapLinearPtr hashmap_linear = module->hashmap_linear;
    unsigned elsize = module->elsize;

    /* Prefetch hashmap_linear entries */
    for (i = 0; i < count; ++i) {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        void *ptr = hashmap_linear_get_copy_key(hashmap_linear, (pkt + 26));

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

        uint32_t *bc = (uint32_t*)(*ptr);
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);

        if (heavyhitter_copy_and_inc(bc, pkt, elsize)) {
            reporter_add_entry(reporter, pkt+26);
        }
    }
}

inline void
heavyhitter_hashmap_linear_ptr_reset(ModulePtr module_) {
    ModuleHeavyHitterHashmapLinearPPtr module = (ModuleHeavyHitterHashmapLinearPPtr)module_;
    HashMapLinearPtr prev = module->hashmap_linear;
    uint8_t *prev_val = module->values;
    reporter_tick(module->reporter);

    module->index = 0;
    if (module->hashmap_linear == module->hashmap_linear_ptr1) {
        module->hashmap_linear = module->hashmap_linear_ptr2;
        module->values = module->vals2;
    } else {
        module->hashmap_linear = module->hashmap_linear_ptr1;
        module->values = module->vals1;
    }

    module->stats_search += hashmap_linear_num_searches(prev);
    memset(prev_val, 0, module->elsize * sizeof(uint32_t) * module->size);
    hashmap_linear_reset(prev);

}

void
heavyhitter_hashmap_linear_ptr_stats(ModulePtr module_, FILE *f) {
    ModuleHeavyHitterHashmapLinearPPtr module = (ModuleHeavyHitterHashmapLinearPPtr)module_;
    module->stats_search += hashmap_linear_num_searches(module->hashmap_linear);
    fprintf(f, "HeavyHitter::Linear::SearchLoad\t%u\n", module->stats_search);
}
