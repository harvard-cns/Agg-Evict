#include <stdio.h>
#include <stdlib.h>

#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "../../reporter.h"

#include "../../dss/countmin.h"
#include "../../vendor/murmur3.h"

#include "common.h"
#include "countmin.h"

ModulePtr heavyhitter_countmin_init(ModuleConfigPtr conf) {
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");

    ReporterPtr reporter = reporter_init(
            mc_uint32_get(conf, "reporter-size"), 
            keysize * 4, socket,
            mc_string_get(conf, "file-prefix"));

    ModuleHeavyHitterCountMinPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleHeavyHitterCountMin), 64, socket); 

    module->_m.execute = heavyhitter_countmin_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;
    module->reporter = reporter;

    module->countmin_ptr1 = countmin_create(size, keysize, elsize, socket);
    module->countmin_ptr2 = countmin_create(size, keysize, elsize, socket);

    module->countmin = module->countmin_ptr1;

    return (ModulePtr)module;
}

void heavyhitter_countmin_delete(ModulePtr module_) {
    ModuleHeavyHitterCountMinPtr module = (ModuleHeavyHitterCountMinPtr)module_;

    countmin_delete(module->countmin_ptr1);
    countmin_delete(module->countmin_ptr2);

    rte_free(module);
}

inline void
heavyhitter_countmin_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) {
    (void)(port);

    ModuleHeavyHitterCountMinPtr module = (ModuleHeavyHitterCountMinPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); (void)(timer);

    CountMinPtr countmin = module->countmin;
    ReporterPtr reporter = module->reporter;

    // printf("fasdfasfdasdf\n");

    /* Prefetch countmin entries */
    for (i = 0; i < count; ++i) {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);

        if (countmin_inc(countmin, (pkt+26), HEAVYHITTER_THRESHOLD)) {
            reporter_add_entry(reporter, pkt+26);
        }
    }
}

inline void
heavyhitter_countmin_reset(ModulePtr module_) {
    ModuleHeavyHitterCountMinPtr module = (ModuleHeavyHitterCountMinPtr)module_;
    CountMinPtr prev = module->countmin;
    reporter_tick(module->reporter);

    if (module->countmin == module->countmin_ptr1) {
        module->countmin = module->countmin_ptr2;
    } else {
        module->countmin = module->countmin_ptr1;
    }

    module->stats_search += countmin_num_searches(prev);
    countmin_reset(prev);
}

inline void
heavyhitter_countmin_stats(ModulePtr module_, FILE *f) {
    ModuleHeavyHitterCountMinPtr module = (ModuleHeavyHitterCountMinPtr)module_;
    module->stats_search += countmin_num_searches(module->countmin);
    fprintf(f, "HeavyHitter::CountMin::SearchLoad\t%u\n", module->stats_search);
}

