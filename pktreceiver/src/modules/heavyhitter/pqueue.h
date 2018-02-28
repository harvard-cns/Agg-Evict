/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Omid Alipourfard <g@omid.io>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _HEAVYHITTER_PQUEUE_H_
#define _HEAVYHITTER_PQUEUE_H_

#include "rte_spinlock.h"

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "../../reporter.h"

#include "../../dss/pqueue.h"

/*
 * Heavyhitter detection with priority queues
 *
 * Keys and values are kept together ... performance is abysmal due to multiple
 * accesses that heap can incur.  If the data-structure is needed, need to use
 * this smartly.
 *
 */
typedef uint32_t Counter;
struct ModuleHeavyHitterPQueue {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    ReporterPtr reporter;
    PriorityQueuePtr pqueue;

    PriorityQueuePtr pqueue_ptr1;
    PriorityQueuePtr pqueue_ptr2;

    rte_spinlock_t lock;
};

typedef struct ModuleHeavyHitterPQueue* ModuleHeavyHitterPQueuePtr;

ModulePtr heavyhitter_pqueue_init(ModuleConfigPtr);

void heavyhitter_pqueue_delete(ModulePtr);
void heavyhitter_pqueue_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void heavyhitter_pqueue_reset(ModulePtr);
void heavyhitter_pqueue_stats(ModulePtr, FILE *);

#endif //PQUEUE
