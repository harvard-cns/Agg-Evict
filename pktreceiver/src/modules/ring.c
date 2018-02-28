#include <stdio.h>
#include <stdlib.h>

#include "rte_common.h"
#include "rte_errno.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"
#include "rte_ring.h"

#include "../common.h"
#include "../vendor/murmur3.h"
#include "../module.h"
#include "../net.h"

#include "ring.h"

ModuleRingPtr ring_init(uint32_t id, uint32_t size,
        uint8_t el_size, uint8_t socket_id) {
    size = rte_align32pow2(size);
    ModuleRingPtr module = rte_zmalloc(0, \
            sizeof(struct ModuleRing) + \
            sizeof(struct ModuleRingHeader) * size, 64); 

    module->_m.execute = ring_execute;
    module->size       = size;
    module->el_size    = el_size;
    module->socket_id  = socket_id;
    module->el_size    = el_size;

    char name[32] = {0};
    snprintf(name, 31, "ring-%d", id);

    module->ring = rte_ring_create(name, size, socket_id, RING_F_SP_ENQ | RING_F_SC_DEQ);

    if (!module->ring) {
        printf("Error is: %s\n", rte_strerror(rte_errno));
        return 0;
    }

    return module;
}

void ring_delete(ModulePtr module) {
    rte_ring_free(((ModuleRingPtr)module)->ring);
    rte_free(module);
}

#define get_ipv4(mbuf) ((struct ipv4_hdr const*) \
        ((rte_pktmbuf_mtod(pkts[i], uint8_t const*)) + \
        (sizeof(struct ether_addr))))

__attribute__((optimize("unroll-loops")))
inline void
ring_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) {
    ModuleRingPtr    module = (ModuleRingPtr)module_;
    struct ModuleRingHeader *table = module->table;
    struct rte_ring *ring = module->ring;
    uint32_t           size = module->size;
    uint32_t i = 0;

    void const *ptr = 0;
    struct ipv4_hdr const *hdr = 0;
    uint32_t table_idx = module->table_idx;

    void *buffers[32];

    if (rte_ring_full(ring)) return;
    if (table_idx + count > size) table_idx = 0;
    struct ModuleRingHeader *table_cell = &table[table_idx];

    for (i = 0; i < count; ++i) {
        hdr = get_ipv4(pkts[i]); 
        ptr = &(hdr->src_addr);

        // Fill up src and dst ip
        rte_mov64((uint8_t*)&table_cell[i], (uint8_t const*)ptr);

        // Fill up the hash
        // MurmurHash3_x86_32(ptr, 8, 1, &(table_cell[i].hash));
        table_cell[i].hash = pkts[i]->hash.rss;

        // save the address
        buffers[i] = &table_cell[i];
    }

    // Enqueue
    rte_ring_enqueue_bulk(ring, buffers, count);

    module->table_idx = table_idx + count;
}

inline struct rte_ring *
ring_get_ring(ModuleRingPtr ring) {
    return ring->ring;
}
