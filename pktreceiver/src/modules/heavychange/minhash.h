/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Yang Zhou <zhou.yang@pku.edu.cn>
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

#ifndef _HEAVYCHANGE_CMSKETCH_H
#define _HEAVYCHANGE_CMSKETCH_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
// #include "../../dss/hashmap_linear.h"
#include "../../dss/hashmap.h"
#include "common.h"

struct MinHash {
    uint32_t size;
    uint32_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;

    HashMapPtr ht_fs; // flow sampling
    
};

typedef struct MinHash* MinHashPtr; 

/* Create a new minhash ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
MinHashPtr minhash_create(uint32_t, uint16_t, uint16_t, int);

inline int  minhash_inc(MinHashPtr, void const *);
inline void minhash_delete(MinHashPtr);
inline void minhash_reset(MinHashPtr);

uint32_t minhash_num_searches(MinHashPtr);





typedef uint32_t Counter;

struct ModuleHeavyChangeMinHash {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    MinHashPtr minhash;

    MinHashPtr minhash_ptr1;
    MinHashPtr minhash_ptr2;
};

typedef struct ModuleHeavyChangeMinHash* ModuleHeavyChangeMinHashPtr;

ModulePtr heavychange_minhash_init(ModuleConfigPtr);

void heavychange_minhash_delete(ModulePtr);
void heavychange_minhash_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void heavychange_minhash_reset(ModulePtr);
void heavychange_minhash_stats(ModulePtr, FILE *f);

#endif

