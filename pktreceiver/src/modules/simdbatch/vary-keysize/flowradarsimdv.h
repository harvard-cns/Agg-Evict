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

#ifndef _SIMDBATCH_FLOWRADAR_SIMD_V_H
#define _SIMDBATCH_FLOWRADAR_SIMD_V_H

#include "../../../common.h"
#include "../../../experiment.h"
#include "../../../module.h"
#include "../../../net.h"
#include "../common.h"
#include "common.h"


struct FlowRadarSimdV {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;

    uint32_t *rowbf;
    CellVPtr rowcell;

    SimdVPtr simdv_ptr;

    uint32_t table[];
};

typedef struct FlowRadarSimdV* FlowRadarSimdVPtr; 

/* Create a new flowradarsimdv ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
FlowRadarSimdVPtr flowradarsimdv_create(uint32_t, uint16_t, uint16_t, int);

inline int  flowradarsimdv_inc_fr(FlowRadarSimdVPtr, void const *, uint32_t);
inline int  flowradarsimdv_inc(FlowRadarSimdVPtr, void const *);
inline void flowradarsimdv_delete(FlowRadarSimdVPtr);
inline void flowradarsimdv_reset(FlowRadarSimdVPtr);

uint32_t flowradarsimdv_num_searches(FlowRadarSimdVPtr);





typedef uint32_t Counter;

struct ModuleSimdBatchFlowRadarSimdV {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    FlowRadarSimdVPtr flowradarsimdv;

    FlowRadarSimdVPtr flowradarsimdv_ptr1;
    FlowRadarSimdVPtr flowradarsimdv_ptr2;
};

typedef struct ModuleSimdBatchFlowRadarSimdV* ModuleSimdBatchFlowRadarSimdVPtr;

ModulePtr simdbatch_flowradarsimdv_init(ModuleConfigPtr);

void simdbatch_flowradarsimdv_delete(ModulePtr);
void simdbatch_flowradarsimdv_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void simdbatch_flowradarsimdv_reset(ModulePtr);
void simdbatch_flowradarsimdv_stats(ModulePtr, FILE *f);

#endif

