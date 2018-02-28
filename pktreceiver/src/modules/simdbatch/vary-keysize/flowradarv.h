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

#ifndef _SIMDBATCH_FLOWRADARV_H
#define _SIMDBATCH_FLOWRADARV_H

#include "../../../common.h"
#include "../../../experiment.h"
#include "../../../module.h"
#include "../../../net.h"
#include "../common.h"
#include "common.h"


struct FlowRadarV {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;

    uint32_t *rowbf;
    CellVPtr rowcell;

    uint32_t table[];
};

typedef struct FlowRadarV* FlowRadarVPtr; 

/* Create a new flowradarv ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
FlowRadarVPtr flowradarv_create(uint32_t, uint16_t, uint16_t, int);

inline int  flowradarv_inc(FlowRadarVPtr, void const *);
inline void flowradarv_delete(FlowRadarVPtr);
inline void flowradarv_reset(FlowRadarVPtr);

uint32_t flowradarv_num_searches(FlowRadarVPtr);





typedef uint32_t Counter;

struct ModuleSimdBatchFlowRadarV {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    FlowRadarVPtr flowradarv;

    FlowRadarVPtr flowradarv_ptr1;
    FlowRadarVPtr flowradarv_ptr2;
};

typedef struct ModuleSimdBatchFlowRadarV* ModuleSimdBatchFlowRadarVPtr;

ModulePtr simdbatch_flowradarv_init(ModuleConfigPtr);

void simdbatch_flowradarv_delete(ModulePtr);
void simdbatch_flowradarv_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void simdbatch_flowradarv_reset(ModulePtr);
void simdbatch_flowradarv_stats(ModulePtr, FILE *f);

#endif

