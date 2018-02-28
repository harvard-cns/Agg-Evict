#include <inttypes.h>
#include <stdlib.h>

#include "rte_malloc.h"
#include "rte_ring.h"

#include "../common.h"
#include "../vendor/murmur3.h"

#include "consumer.h"
#include "ring.h"

struct Consumer {
    struct rte_ring *rings[CONSUMER_MAX_RINGS];
    int nrings;
    uint64_t packets;

    uint64_t table[];
};

ConsumerPtr consumer_init(void) {
    ConsumerPtr consumer = rte_zmalloc(0, sizeof(struct Consumer) + \
            sizeof(uint64_t) * COUNT_ARRAY_SIZE, 64);
    return consumer;
}

inline int
consumer_add_ring(ConsumerPtr cn, struct rte_ring *ring) {
    if (cn->nrings >= CONSUMER_MAX_RINGS) return -1;

    cn->rings[cn->nrings++] = ring;
    return 0;
}

inline void
consumer_delete(ConsumerPtr cn) {
    rte_free(cn);
}

inline void
consumer_loop(ConsumerPtr cn) {
    unsigned nring = 0;
    unsigned nrings = cn->nrings;
    unsigned i = 0;
    unsigned const nburst = CONSUMER_BURST_SIZE - 1;

    void *buffer[CONSUMER_BURST_SIZE] = {0};
    uint64_t *table = cn->table;
    uint32_t hash = 0;
    while (1) {
        for (nring = 0; nring < nrings; ++nring) {
            struct rte_ring *ring = cn->rings[nring];
            unsigned ret = rte_ring_dequeue_bulk(ring, buffer, nburst);
            if (ret) continue;

            for (i = 0; i < nburst; ++i) {
                struct ModuleRingHeader *hdr = 
                    (struct ModuleRingHeader *)buffer[i];

                MurmurHash3_x86_32(hdr, 8, 1, &hash);
                table[hash & COUNT_ARRAY_SIZE]++;

                MurmurHash3_x86_32(hdr, 8, 1, &hash);
                table[hash & COUNT_ARRAY_SIZE]++;

                MurmurHash3_x86_32(hdr, 8, 1, &hash);
                table[hash & COUNT_ARRAY_SIZE]++;
                //table[hdr->hash & COUNT_ARRAY_SIZE]++;
            }

            cn->packets += nburst;
        }
    }
}

inline void
consumer_print_stats(ConsumerPtr cn) {
    printf("Number of packets: %"PRIu64"\n", cn->packets);
}
