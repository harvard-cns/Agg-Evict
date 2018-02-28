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

#ifndef _SIMDBATCH_FLOWRADAR_H
#define _SIMDBATCH_FLOWRADAR_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "common.h"


struct FlowRadar {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;

    uint32_t *rowbf;
    CellPtr rowcell;

    uint32_t table[];
};

typedef struct FlowRadar* FlowRadarPtr; 

/* Create a new flowradar ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
FlowRadarPtr flowradar_create(uint32_t, uint16_t, uint16_t, int);

inline int  flowradar_inc(FlowRadarPtr, void const *);
inline void flowradar_delete(FlowRadarPtr);
inline void flowradar_reset(FlowRadarPtr);

uint32_t flowradar_num_searches(FlowRadarPtr);





typedef uint32_t Counter;

struct ModuleSimdBatchFlowRadar {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    FlowRadarPtr flowradar;

    FlowRadarPtr flowradar_ptr1;
    FlowRadarPtr flowradar_ptr2;
};

typedef struct ModuleSimdBatchFlowRadar* ModuleSimdBatchFlowRadarPtr;

ModulePtr simdbatch_flowradar_init(ModuleConfigPtr);

void simdbatch_flowradar_delete(ModulePtr);
void simdbatch_flowradar_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void simdbatch_flowradar_reset(ModulePtr);
void simdbatch_flowradar_stats(ModulePtr, FILE *f);

#endif

