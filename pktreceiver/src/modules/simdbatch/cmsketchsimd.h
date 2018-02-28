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

#ifndef _SIMDBATCH_CMSKETCHSIMD_H
#define _SIMDBATCH_CMSKETCHSIMD_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "common.h"


struct CMSketchSimd {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;

    uint32_t *row1;
    uint32_t *row2;
    uint32_t *row3;
    uint32_t *row4;

    SimdPtr simd_ptr;

    uint32_t table[];

};

typedef struct CMSketchSimd* CMSketchSimdPtr; 

/* Create a new cmsketchsimd ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
CMSketchSimdPtr cmsketchsimd_create(uint32_t, uint16_t, uint16_t, int);

inline int cmsketchsimd_inc_cm(CMSketchSimdPtr, void const *, uint32_t);
inline int  cmsketchsimd_inc(CMSketchSimdPtr, void const *);
inline void cmsketchsimd_delete(CMSketchSimdPtr);
inline void cmsketchsimd_reset(CMSketchSimdPtr);

uint32_t cmsketchsimd_num_searches(CMSketchSimdPtr);





typedef uint32_t Counter;

struct ModuleSimdBatchCMSketchSimd {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    CMSketchSimdPtr cmsketchsimd;

    CMSketchSimdPtr cmsketchsimd_ptr1;
    CMSketchSimdPtr cmsketchsimd_ptr2;
};

typedef struct ModuleSimdBatchCMSketchSimd* ModuleSimdBatchCMSketchSimdPtr;

ModulePtr simdbatch_cmsketchsimd_init(ModuleConfigPtr);

void simdbatch_cmsketchsimd_delete(ModulePtr);
void simdbatch_cmsketchsimd_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void simdbatch_cmsketchsimd_reset(ModulePtr);
void simdbatch_cmsketchsimd_stats(ModulePtr, FILE *f);

#endif
