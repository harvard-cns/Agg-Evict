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

#ifndef _HEAVYCHANGE_PKTTEST_H
#define _HEAVYCHANGE_PKTTEST_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "common.h"

struct PktTest {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;
    uint32_t cur_size;
    // uint32_t epoch_id;

    rte_atomic32_t stats_search;

    uint64_t *row_key;


    uint32_t table[];
};

typedef struct PktTest* PktTestPtr; 

/* Create a new pkttest ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
PktTestPtr pkttest_create(uint32_t, uint16_t, uint16_t, int);

inline int  pkttest_inc(PktTestPtr, void const *);
inline void pkttest_delete(PktTestPtr);
inline void pkttest_reset(PktTestPtr);

uint32_t pkttest_num_searches(PktTestPtr);





typedef uint32_t Counter;

struct ModuleHeavyChangePktTest {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    PktTestPtr pkttest;

    PktTestPtr pkttest_ptr1;
    PktTestPtr pkttest_ptr2;
};

typedef struct ModuleHeavyChangePktTest* ModuleHeavyChangePktTestPtr;

ModulePtr heavychange_pkttest_init(ModuleConfigPtr);

void heavychange_pkttest_delete(ModulePtr);
void heavychange_pkttest_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void heavychange_pkttest_reset(ModulePtr);
void heavychange_pkttest_stats(ModulePtr, FILE *f);

#endif

