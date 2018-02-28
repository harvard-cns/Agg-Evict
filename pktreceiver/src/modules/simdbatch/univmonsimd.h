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

#ifndef _SIMDBATCH_UNIVMON_SIMD_H
#define _SIMDBATCH_UNIVMON_SIMD_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "common.h"
#include "countheap.h"


struct UnivMonSimd {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;

    CountHeapPtr CH1;
    CountHeapPtr CH2;
    CountHeapPtr CH3;
    CountHeapPtr CH4;

    SimdPtr simd_ptr;
};

typedef struct UnivMonSimd* UnivMonSimdPtr; 

/* Create a new univmonsimd ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
UnivMonSimdPtr univmonsimd_create(uint32_t, uint16_t, uint16_t, int);

inline int univmonsimd_inc_um(UnivMonSimdPtr, void const *, uint32_t);
inline int  univmonsimd_inc(UnivMonSimdPtr, void const *);
inline void univmonsimd_delete(UnivMonSimdPtr);
inline void univmonsimd_reset(UnivMonSimdPtr);

uint32_t univmonsimd_num_searches(UnivMonSimdPtr);





typedef uint32_t Counter;

struct ModuleSimdBatchUnivMonSimd {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    UnivMonSimdPtr univmonsimd;

    UnivMonSimdPtr univmonsimd_ptr1;
    UnivMonSimdPtr univmonsimd_ptr2;
};

typedef struct ModuleSimdBatchUnivMonSimd* ModuleSimdBatchUnivMonSimdPtr;

ModulePtr simdbatch_univmonsimd_init(ModuleConfigPtr);

void simdbatch_univmonsimd_delete(ModulePtr);
void simdbatch_univmonsimd_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void simdbatch_univmonsimd_reset(ModulePtr);
void simdbatch_univmonsimd_stats(ModulePtr, FILE *f);

#endif

