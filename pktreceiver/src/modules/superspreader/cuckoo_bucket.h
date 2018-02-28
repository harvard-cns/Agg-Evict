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

#ifndef _SUPERSPREADER_CUCKOO_BUCKET_H_
#define _SUPERSPREADER_CUCKOO_BUCKET_H_

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "../../reporter.h"

#include "../../dss/bloomfilter.h"
#include "../../dss/hashmap_cuckoo_bucket.h"

/*
 * Heavyhitter detection with a local implementation of Cuckoo hashing
 *
 * The keys and values are saved sequentially after each other.  There is
 * no bucketing with this implementation, so the performance is limited by
 * the access pattern, which is mostly random due to hash functions.
 *
 */
typedef uint32_t Counter;
struct ModuleSuperSpreaderCuckooB {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    uint32_t stats_search;

    ReporterPtr reporter;

    struct BFProp bfprop;
    HashMapCuckooBucketPtr hashmap;

    HashMapCuckooBucketPtr hashmap_ptr1;
    HashMapCuckooBucketPtr hashmap_ptr2;

    uint8_t *values;
    uint8_t *val1;
    uint8_t *val2;

    uint32_t idx1;
    uint32_t idx2;
    uint32_t *index;
};

typedef struct ModuleSuperSpreaderCuckooB* ModuleSuperSpreaderCuckooBPtr;
ModulePtr superspreader_cuckoo_bucket_init(ModuleConfigPtr params);

void superspreader_cuckoo_bucket_delete(ModulePtr);
void superspreader_cuckoo_bucket_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void superspreader_cuckoo_bucket_reset(ModulePtr);
void superspreader_cuckoo_bucket_stats(ModulePtr, FILE*);

#endif
