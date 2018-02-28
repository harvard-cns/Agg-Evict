#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "rte_common.h"
#include "rte_cycles.h"
#include "rte_eal.h"
#include "rte_ethdev.h"
#include "rte_errno.h"
#include "rte_mempool.h"

#include "common.h"
#include "memory.h"

#include "console.h"

struct Console {
    uint32_t display_rate_ms;
    uint64_t last_tick;
    uint64_t freq;
    uint64_t cycles_threshold;
};

ConsolePtr
console_create(uint32_t display_rate_ms) {
    ConsolePtr console = 0;
    mem_alloc(sizeof(struct Console), 0, (void**)&console);

    if (!console)
        return 0;

    console->display_rate_ms = display_rate_ms;
    console->last_tick = rte_get_tsc_cycles();
    console->freq = rte_get_tsc_hz();
    console->cycles_threshold = (console->freq / 1000) * display_rate_ms;

    return console;
}

inline bool
console_refresh(ConsolePtr console) {
    uint64_t l_tick = rte_get_tsc_cycles();
    
    if (unlikely(l_tick - console->last_tick > console->cycles_threshold)) {
        console->last_tick = l_tick;
        return true;
    }

    return false;
}


