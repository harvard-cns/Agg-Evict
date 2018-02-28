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

#ifndef _SIMDBATCH_LINEAR_COUNTING_H
#define _SIMDBATCH_LINEAR_COUNTING_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "common.h"

struct LinearCounting {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;

    uint32_t *row1;
    uint32_t *row2;
    uint32_t *row3;
    uint32_t *row4;


    uint32_t table[];
};

typedef struct LinearCounting* LinearCountingPtr; 

/* Create a new linearcounting ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
LinearCountingPtr linearcounting_create(uint32_t, uint16_t, uint16_t, int);

inline int  linearcounting_inc(LinearCountingPtr, void const *);
inline void linearcounting_delete(LinearCountingPtr);
inline void linearcounting_reset(LinearCountingPtr);

uint32_t linearcounting_num_searches(LinearCountingPtr);





typedef uint32_t Counter;

struct ModuleSimdBatchLinearCounting {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    LinearCountingPtr linearcounting;

    LinearCountingPtr linearcounting_ptr1;
    LinearCountingPtr linearcounting_ptr2;
};

typedef struct ModuleSimdBatchLinearCounting* ModuleSimdBatchLinearCountingPtr;

ModulePtr simdbatch_linearcounting_init(ModuleConfigPtr);

void simdbatch_linearcounting_delete(ModulePtr);
void simdbatch_linearcounting_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void simdbatch_linearcounting_reset(ModulePtr);
void simdbatch_linearcounting_stats(ModulePtr, FILE *f);

#endif

