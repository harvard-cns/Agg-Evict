#include <stdio.h>
#include <stdlib.h>

#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_hash.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"

#include "../common.h"
#include "../vendor/murmur3.h"
#include "../module.h"
#include "../net.h"
#include "../pkt.h"
#include "../dss/hash.h"

#include "randmod.h"

ModulePtr randmod_init(ModuleConfigPtr conf) {
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t accsize = mc_uint32_get(conf, "accsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleRandModPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleRandMod) + sizeof(uint32_t) * size,
            64, socket); 

    module->_m.execute = randmod_execute;
    module->size = size;
    module->access_size = accsize;
    module->socket = socket;

    return (ModulePtr)module;
}

void randmod_delete(ModulePtr module_) {
    // ModuleRandModPtr module  = (ModuleRandModPtr)module_;
    rte_free(module_);
}

void randmod_execute(ModulePtr module_,
    PortPtr port __attribute__((unused)),
    struct rte_mbuf **pkts __attribute__((unused)),
    uint32_t count) {
    ModuleRandModPtr module  = (ModuleRandModPtr)module_;
    unsigned i = 0;
    uint32_t *ptrs[MAX_PKT_BURST];
    uint32_t idx = module->idx;
    uint32_t *tbl = (uint32_t*)module->values;
    uint32_t size = module->size;
    
    for (i = 0; i < count; ++i) {
        unsigned hashVal = idx + i;
        uint32_t hash = (dss_hash(&hashVal, 1) & size);
        ptrs[i] = (tbl + hash);
        rte_prefetch0(tbl + hash);
    }

    for (i = 0; i < count; ++i) {
        (*ptrs[i])++;
    }

    module->idx += count;
}

void randmod_reset(ModulePtr module_) {
    ModuleRandModPtr module  = (ModuleRandModPtr)module_;
    memset(module->values, 0, module->size * sizeof(uint32_t));
    module->idx = 0;
}

void randmod_stats(ModulePtr module_, FILE *f) {
    (void)(f);
    (void)(module_);
}
