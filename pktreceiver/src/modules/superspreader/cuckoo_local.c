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

#include "../../dss/bloomfilter.h"
#include "../../dss/hashmap_cuckoo.h"

#include "common.h"
#include "cuckoo_local.h"

ModulePtr superspreader_cuckoo_local_init(ModuleConfigPtr conf) {
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t socket  = mc_uint32_get(conf, "socket");

    ReporterPtr reporter = reporter_init(
            mc_uint32_get(conf, "reporter-size"), 
            keysize * 4, socket,
            mc_string_get(conf, "file-prefix"));

    ModuleSuperSpreaderCuckooLPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSuperSpreaderCuckooL), 64, socket); 

    module->_m.execute = superspreader_cuckoo_local_execute;
    module->size  = size;
    module->keysize = keysize;
    module->bfprop.keylen = keysize;
    module->elsize = BF_SIZE/(8*4) + 1; // Keysize + bloomfilter size
    module->socket = socket;
    module->reporter = reporter;

    module->hashmap_ptr1 = hashmap_cuckoo_create(size, keysize, module->elsize, socket);
    module->hashmap_ptr2 = hashmap_cuckoo_create(size, keysize, module->elsize, socket);

    module->hashmap = module->hashmap_ptr1;

    return (ModulePtr)module;
}

void superspreader_cuckoo_local_delete(ModulePtr module_) {
    ModuleSuperSpreaderCuckooLPtr module = (ModuleSuperSpreaderCuckooLPtr)module_;

    hashmap_cuckoo_delete(module->hashmap_ptr1);
    hashmap_cuckoo_delete(module->hashmap_ptr2);

    rte_free(module);
}

inline void
superspreader_cuckoo_local_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) {
    (void)(port);

    ModuleSuperSpreaderCuckooLPtr module = (ModuleSuperSpreaderCuckooLPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); (void)(timer);
    void *ptrs[MAX_PKT_BURST];
    HashMapCuckooPtr hashmap = module->hashmap;

    /* Prefetch hashmap entries */
    for (i = 0; i < count; ++i) {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        void *ptr = hashmap_cuckoo_get_copy_key(hashmap, (pkt + 26));

        ptrs[i] = ptr;
        rte_prefetch0(ptr);
    }

    BFPropPtr bfptr = &module->bfprop;
    unsigned keysize = module->keysize;

    /* Save and report if necessary */
    ReporterPtr reporter = module->reporter;
    for (i = 0; i < count; ++i) { 
        void *ptr = ptrs[i];
        uint32_t *bc = (uint32_t*)(ptr);
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);

        if (superspreader_copy_and_inc(bfptr, bc, pkt, keysize)) {
            reporter_add_entry(reporter, pkt+26);
        }
    }
}

inline void
superspreader_cuckoo_local_reset(ModulePtr module_) {
    ModuleSuperSpreaderCuckooLPtr module = (ModuleSuperSpreaderCuckooLPtr)module_;
    HashMapCuckooPtr prev = module->hashmap;
    reporter_tick(module->reporter);

    if (module->hashmap == module->hashmap_ptr1) {
        module->hashmap = module->hashmap_ptr2;
    } else {
        module->hashmap = module->hashmap_ptr1;
    }

    module->stats_search += hashmap_cuckoo_num_searches(prev);
    hashmap_cuckoo_reset(prev);
}

inline void
superspreader_cuckoo_local_stats(ModulePtr module_, FILE *f) {
    ModuleSuperSpreaderCuckooLPtr module = (ModuleSuperSpreaderCuckooLPtr)module_;
    module->stats_search += hashmap_cuckoo_num_searches(module->hashmap);
    fprintf(f, "SuperSpreader::Cuckoo::SearchLoad\t%u\n", module->stats_search);
}



