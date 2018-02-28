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

#ifndef _SIMDBATCH_FMSKETCH_H
#define _SIMDBATCH_FMSKETCH_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "common.h"

struct FMSketch {
    rte_atomic32_t stats_search;
    uint16_t keysize;

    uint32_t count1;
    uint32_t count2;
    uint32_t count3;
    uint32_t count4;
};

typedef struct FMSketch* FMSketchPtr; 

/* Create a new fmsketch ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
FMSketchPtr fmsketch_create(uint16_t, int);

inline int  fmsketch_inc(FMSketchPtr, void const *);
inline void fmsketch_delete(FMSketchPtr);
inline void fmsketch_reset(FMSketchPtr);

uint32_t fmsketch_num_searches(FMSketchPtr);





typedef uint32_t Counter;

struct ModuleSimdBatchFMSketch {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    FMSketchPtr fmsketch;

    FMSketchPtr fmsketch_ptr1;
    FMSketchPtr fmsketch_ptr2;
};

typedef struct ModuleSimdBatchFMSketch* ModuleSimdBatchFMSketchPtr;

ModulePtr simdbatch_fmsketch_init(ModuleConfigPtr);

void simdbatch_fmsketch_delete(ModulePtr);
void simdbatch_fmsketch_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void simdbatch_fmsketch_reset(ModulePtr);
void simdbatch_fmsketch_stats(ModulePtr, FILE *f);

#endif

