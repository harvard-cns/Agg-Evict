#include <stdio.h>
#include <stdlib.h>

#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"
#include "rte_spinlock.h"

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "../../reporter.h"

#include "../../dss/pqueue.h"
#include "../../vendor/murmur3.h"

#include "common.h"
#include "pqueue.h"

static int
counter_comp(void const *ap, void const *bp) {
    uint32_t a = (*(uint32_t const*)ap);
    uint32_t b = (*(uint32_t const*)bp);
    //printf("Comparing: %d vs. %d: %d\n", a, b, ((a >= b) - (a <= b)));
    return -((a >= b) - (a <= b));
}

ModulePtr heavyhitter_pqueue_init(ModuleConfigPtr conf) {
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");

    ReporterPtr reporter = reporter_init(
            mc_uint32_get(conf, "reporter-size"), 
            keysize * 4, socket,
            mc_string_get(conf, "file-prefix"));

    ModuleHeavyHitterPQueuePtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleHeavyHitterPQueue), 64, socket); 


    /* XXX: we have to save the keys with the values ... rte_hash */
    elsize = keysize + elsize;

    module->_m.execute = heavyhitter_pqueue_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;
    module->reporter = reporter;
    rte_spinlock_init(&module->lock);

    module->pqueue_ptr1 = pqueue_create(size, keysize, elsize, socket, counter_comp);
    module->pqueue_ptr2 = pqueue_create(size, keysize, elsize, socket, counter_comp);

    module->pqueue = module->pqueue_ptr1;

    return (ModulePtr)module;
}

void heavyhitter_pqueue_delete(ModulePtr module_) {
    ModuleHeavyHitterPQueuePtr module = (ModuleHeavyHitterPQueuePtr)module_;

    pqueue_delete(module->pqueue_ptr1);
    pqueue_delete(module->pqueue_ptr2);

    rte_free(module);
}

#define get_ipv4(mbuf) ((struct ipv4_hdr const*) \
        ((rte_pktmbuf_mtod(pkts[i], uint8_t const*)) + \
        (sizeof(struct ether_addr))))

inline void
heavyhitter_pqueue_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) {
    (void)(port);

    ModuleHeavyHitterPQueuePtr module = (ModuleHeavyHitterPQueuePtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); (void)(timer);
    pqueue_index_t idx;
    uint16_t keysize = module->keysize;
    unsigned elsize  = module->elsize;

    rte_spinlock_lock(&module->lock);

    /* Prefetch pqueue entries */
    for (i = 0; i < count; ++i) {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        void *ptr = pqueue_get_copy_key(module->pqueue, (pkt + 26), &idx);

        /* Layout of each cell in the PQueue is:
         * 0 : [ key ]: [src/dst ip]
         * 8 : [count]
         * 12: [value]: [pkt-data until elsize is satisfied]
         */
        uint32_t *bc = (uint32_t*)(ptr);
        rte_memcpy(bc, pkt+26, sizeof(uint32_t) * keysize);
        bc += keysize; (*bc)++;
        rte_memcpy((bc + 1), pkt, (elsize-keysize-1)*4);
        pqueue_heapify_index(module->pqueue, idx);
    }

    rte_spinlock_unlock(&module->lock);
}

static void
save_heavy_hitters(PriorityQueuePtr pq, ReporterPtr reporter, uint16_t keysize) {
    void *cell = 0; pqueue_index_t idx = 0;
    while ((cell = pqueue_iterate(pq, &idx)) != 0) {
        uint32_t count = *((uint32_t*)cell+keysize);

        if (count > HEAVYHITTER_THRESHOLD) {
            reporter_add_entry(reporter, ((uint32_t*)cell));
        }
    }
    reporter_tick(reporter);

}

inline void
heavyhitter_pqueue_reset(ModulePtr module_) {
    ModuleHeavyHitterPQueuePtr module = (ModuleHeavyHitterPQueuePtr)module_;
    PriorityQueuePtr prev = module->pqueue;

    if (module->pqueue == module->pqueue_ptr1) {
        module->pqueue = module->pqueue_ptr2;
    } else {
        module->pqueue = module->pqueue_ptr1;
    }

    save_heavy_hitters(prev, module->reporter, module->keysize);

    rte_spinlock_lock(&module->lock);
    module->stats_search += pqueue_num_searches(prev);
    pqueue_reset(prev);
    rte_spinlock_unlock(&module->lock);
}

inline void
heavyhitter_pqueue_stats(ModulePtr module_, FILE *f) {
    ModuleHeavyHitterPQueuePtr module = (ModuleHeavyHitterPQueuePtr)module_;
    module->stats_search += pqueue_num_searches(module->pqueue);
    fprintf(f, "HeavyHitter::PQueue::SearchLoad\t%u\n", module->stats_search);
}
