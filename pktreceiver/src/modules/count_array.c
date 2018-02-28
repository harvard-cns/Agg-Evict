#include <stdio.h>
#include <stdlib.h>

#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"

#include "../common.h"
#include "../vendor/murmur3.h"
#include "../module.h"
#include "../net.h"

#include "count_array.h"

ModuleCountArrayPtr count_array_init(uint32_t size) {
    ModuleCountArrayPtr module = rte_zmalloc(0,
                sizeof(struct Module) + 
                sizeof(uint32_t) + 
                sizeof(uint32_t) * MAX_PKT_BURST + 
                size * sizeof(Counter), 64); 

    module->_m.execute = count_array_execute;
    module->size  = size;
    return module;
}

void count_array_delete(ModulePtr module) {
    rte_free(module);
}

#define get_ipv4(mbuf) ((struct ipv4_hdr const*) \
        ((rte_pktmbuf_mtod(pkts[i], uint8_t const*)) + \
        (sizeof(struct ether_addr))))

__attribute__((optimize("unroll-loops")))
inline void
count_array_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) {
    ModuleCountArrayPtr module = (ModuleCountArrayPtr)module_;
    uint32_t size = module->size;
    Counter *table = module->table;
    uint32_t *hashes = module->hashes;
    uint32_t i = 0, hash = 0;
    void const *ptr = 0;
    struct ipv4_hdr const *hdr = 0;

    for (i = 0; i < count; ++i) {
        rte_prefetch0(pkts[i]);
    }

    for (i = 0; i < count; ++i) {
        hdr = get_ipv4(pkts[i]); 
        ptr = &(hdr->src_addr);
        //hash = pkts[i]->hash.rss;
        MurmurHash3_x86_32(ptr, 8, 1, &hash);
        hash = hash & size;
        rte_prefetch0(&table[hash]);
        hashes[i] = hash;
    }

    for (i = 0; i < count; ++i) {
        table[hashes[i]]++;
    }
}

