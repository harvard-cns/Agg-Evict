#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"

#include "../common.h"
#include "../vendor/murmur3.h"
#include "../module.h"
#include "../net.h"

#include "super_spreader.h"


ModuleSuperSpreaderPtr super_spreader_init(uint32_t size) {
    ModuleSuperSpreaderPtr module = rte_zmalloc(0,
                sizeof(struct Module) + 
                sizeof(uint32_t) + 
                sizeof(uint32_t) * MAX_PKT_BURST + 
                size * sizeof(struct SSField), 64); 
    module->_m.execute = super_spreader_execute;
    module->size  = size;
    return module;
}

void super_spreader_delete(ModulePtr module) {
    rte_free(module);
}

#define get_ipv4(mbuf) ((struct ipv4_hdr const*) \
        ((rte_pktmbuf_mtod(pkts[i], uint8_t const*)) + \
        (sizeof(struct ether_addr))))

#define bit_hash(bit, ip) {\
            bit = (ip[0] ^ ip[1] ^ ip[2] ^ ip[3]);\
            bit ^= (bit >> 2);\
            bit &= 63;\
        }

__attribute__((optimize("unroll-loops")))
inline void
super_spreader_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) {
    ModuleSuperSpreaderPtr module = (ModuleSuperSpreaderPtr)module_;
    uint32_t size = module->size;
    struct SSField *table = module->table;
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
        MurmurHash3_x86_32(ptr, 4, 1, &hash);
        hash = hash & size;
        rte_prefetch0(&table[hash]);
        hashes[i] = hash;
    }

    uint32_t srcip = 0;
    uint8_t const *dstip;
    struct SSField *field = 0;
    uint8_t  bit = 0;
    uint64_t bit_field = 0;
    uint64_t seen = 0;
    for (i = 0; i < count; ++i) {
        field = &table[hashes[i]];
        hdr = get_ipv4(pkts[i]); 
        srcip = hdr->src_addr;
        dstip = (uint8_t const*)&(hdr->dst_addr);

        field->src = srcip;
        bit_hash(bit, dstip);
        bit_field = 1 << bit;
        seen = field->bits & bit_field;
        field->bits |= bit_field;
        field->count += (seen == 0);

        if (field->count > 32)
            printf("Superspreader: %d, %d\n", srcip, field->count);
    }
}
