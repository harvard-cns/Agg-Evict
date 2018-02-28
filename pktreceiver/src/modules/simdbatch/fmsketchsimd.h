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

#ifndef _SIMDBATCH_FMSKETCH_SIMD_H
#define _SIMDBATCH_FMSKETCH_SIMD_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "common.h"

struct FMSketchSimd {
    rte_atomic32_t stats_search;
    uint16_t keysize;

    uint32_t count1;
    uint32_t count2;
    uint32_t count3;
    uint32_t count4;

    SimdPtr simd_ptr;
};

typedef struct FMSketchSimd* FMSketchSimdPtr; 

/* Create a new fmsketchsimd ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
FMSketchSimdPtr fmsketchsimd_create(uint16_t, int);

inline int fmsketchsimd_inc_fm(FMSketchSimdPtr, void const *);
inline int  fmsketchsimd_inc(FMSketchSimdPtr, void const *);
inline void fmsketchsimd_delete(FMSketchSimdPtr);
inline void fmsketchsimd_reset(FMSketchSimdPtr);

uint32_t fmsketchsimd_num_searches(FMSketchSimdPtr);





typedef uint32_t Counter;

struct ModuleSimdBatchFMSketchSimd {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    FMSketchSimdPtr fmsketchsimd;

    FMSketchSimdPtr fmsketchsimd_ptr1;
    FMSketchSimdPtr fmsketchsimd_ptr2;
};

typedef struct ModuleSimdBatchFMSketchSimd* ModuleSimdBatchFMSketchSimdPtr;

ModulePtr simdbatch_fmsketchsimd_init(ModuleConfigPtr);

void simdbatch_fmsketchsimd_delete(ModulePtr);
void simdbatch_fmsketchsimd_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void simdbatch_fmsketchsimd_reset(ModulePtr);
void simdbatch_fmsketchsimd_stats(ModulePtr, FILE *f);

#endif

