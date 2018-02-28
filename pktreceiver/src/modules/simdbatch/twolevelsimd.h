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

#ifndef _SIMDBATCH_TWOLEVEL_SIMD_H
#define _SIMDBATCH_TWOLEVEL_SIMD_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "common.h"

struct TwoLevelSimd {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;

    BitCellPtr rowrev1;
    BitCellPtr rowrev2;
    BitCellPtr rowrev3;
    BitCellPtr rowrev4;

    BitCellPtr rowcm1;
    BitCellPtr rowcm2;
    BitCellPtr rowcm3;
    BitCellPtr rowcm4;

    SimdSDPtr simd_ptr;

    uint32_t table[];
};

typedef struct TwoLevelSimd* TwoLevelSimdPtr; 

/* Create a new twolevelsimd ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
TwoLevelSimdPtr twolevelsimd_create(uint32_t, uint16_t, uint16_t, int);

inline int  twolevelsimd_inc_tl(TwoLevelSimdPtr, void const *);
inline int  twolevelsimd_inc(TwoLevelSimdPtr, void const *);
inline void twolevelsimd_delete(TwoLevelSimdPtr);
inline void twolevelsimd_reset(TwoLevelSimdPtr);

uint32_t twolevelsimd_num_searches(TwoLevelSimdPtr);





typedef uint32_t Counter;

struct ModuleSimdBatchTwoLevelSimd {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    TwoLevelSimdPtr twolevelsimd;

    TwoLevelSimdPtr twolevelsimd_ptr1;
    TwoLevelSimdPtr twolevelsimd_ptr2;
};

typedef struct ModuleSimdBatchTwoLevelSimd* ModuleSimdBatchTwoLevelSimdPtr;

ModulePtr simdbatch_twolevelsimd_init(ModuleConfigPtr);

void simdbatch_twolevelsimd_delete(ModulePtr);
void simdbatch_twolevelsimd_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void simdbatch_twolevelsimd_reset(ModulePtr);
void simdbatch_twolevelsimd_stats(ModulePtr, FILE *f);

#endif

