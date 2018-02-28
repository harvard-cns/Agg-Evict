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

#include "../../dss/countarray.h"
#include "../../vendor/murmur3.h"

#include "common.h"
#include "countarray.h"

ModulePtr heavyhitter_countarray_init(ModuleConfigPtr conf) {
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");

    ReporterPtr reporter = reporter_init(
            mc_uint32_get(conf, "reporter-size"), 
            keysize * 4, socket,
            mc_string_get(conf, "file-prefix"));

    ModuleHeavyHitterCountArrayPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleHeavyHitterCountArray), 64, socket); 

    module->_m.execute = heavyhitter_countarray_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;
    module->reporter = reporter;

    module->countarray_ptr1 = countarray_create(size, keysize, elsize, socket);
    module->countarray_ptr2 = countarray_create(size, keysize, elsize, socket);

    module->countarray = module->countarray_ptr1;

    return (ModulePtr)module;
}

void heavyhitter_countarray_delete(ModulePtr module_) {
    ModuleHeavyHitterCountArrayPtr module = (ModuleHeavyHitterCountArrayPtr)module_;

    countarray_delete(module->countarray_ptr1);
    countarray_delete(module->countarray_ptr2);

    rte_free(module);
}

inline void
heavyhitter_countarray_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) {
    (void)(port);

    ModuleHeavyHitterCountArrayPtr module = (ModuleHeavyHitterCountArrayPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); (void)(timer);

    CountArrayPtr countarray = module->countarray;
    ReporterPtr reporter = module->reporter;

    // printf("fasdfasfdasdf\n");

    /* Prefetch countarray entries */
    for (i = 0; i < count; ++i) {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);

        if (countarray_inc(countarray, (pkt+26), HEAVYHITTER_THRESHOLD)) {
            reporter_add_entry(reporter, pkt+26);
        }
    }
}

inline void
heavyhitter_countarray_reset(ModulePtr module_) {
    ModuleHeavyHitterCountArrayPtr module = (ModuleHeavyHitterCountArrayPtr)module_;
    CountArrayPtr prev = module->countarray;
    reporter_tick(module->reporter);

    if (module->countarray == module->countarray_ptr1) {
        module->countarray = module->countarray_ptr2;
    } else {
        module->countarray = module->countarray_ptr1;
    }

    module->stats_search += countarray_num_searches(prev);
    countarray_reset(prev);
}

inline void
heavyhitter_countarray_stats(ModulePtr module_, FILE *f) {
    ModuleHeavyHitterCountArrayPtr module = (ModuleHeavyHitterCountArrayPtr)module_;
    module->stats_search += countarray_num_searches(module->countarray);
    fprintf(f, "HeavyHitter::CountArray::SearchLoad\t%u\n", module->stats_search);
}

