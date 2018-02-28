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

#ifndef _SUPERSPREADER_CUCKOO_LOCAL_H_
#define _SUPERSPREADER_CUCKOO_LOCAL_H_

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "../../reporter.h"

#include "../../dss/bloomfilter.h"
#include "../../dss/hashmap_cuckoo.h"

/*
 * Heavyhitter detection with a local implementation of Cuckoo hashing
 *
 * The keys and values are saved sequentially after each other.  There is
 * no bucketing with this implementation, so the performance is limited by
 * the access pattern, which is mostly random due to hash functions.
 *
 */
typedef uint32_t Counter;
struct ModuleSuperSpreaderCuckooL {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    uint32_t stats_search;

    ReporterPtr reporter;

    struct BFProp bfprop;
    HashMapCuckooPtr hashmap;

    HashMapCuckooPtr hashmap_ptr1;
    HashMapCuckooPtr hashmap_ptr2;
};

typedef struct ModuleSuperSpreaderCuckooL* ModuleSuperSpreaderCuckooLPtr;
ModulePtr superspreader_cuckoo_local_init(ModuleConfigPtr params);

void superspreader_cuckoo_local_delete(ModulePtr);
void superspreader_cuckoo_local_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void superspreader_cuckoo_local_reset(ModulePtr);
void superspreader_cuckoo_local_stats(ModulePtr, FILE*);

#endif
