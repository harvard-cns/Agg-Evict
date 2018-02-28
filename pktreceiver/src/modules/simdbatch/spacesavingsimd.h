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

#ifndef _SIMDBATCH_SPACESAVING_SIMD_H
#define _SIMDBATCH_SPACESAVING_SIMD_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "common.h"



struct SpaceSavingSimd {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;


    uint32_t now_element;
    NodePtr nodes;

    SimdPtr simd_ptr;

    uint32_t table[];
};

typedef struct SpaceSavingSimd* SpaceSavingSimdPtr; 

/* Create a new spacesavingsimd ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
SpaceSavingSimdPtr spacesavingsimd_create(uint32_t, uint16_t, uint16_t, int);

inline int spacesavingsimd_inc_ss(SpaceSavingSimdPtr, void const *, uint32_t);
inline int  spacesavingsimd_inc(SpaceSavingSimdPtr, void const *);
inline void spacesavingsimd_delete(SpaceSavingSimdPtr);
inline void spacesavingsimd_reset(SpaceSavingSimdPtr);

uint32_t spacesavingsimd_num_searches(SpaceSavingSimdPtr);


inline void swap_32_ss_simd(uint32_t *a, uint32_t *b);
inline void swap_64_ss_simd(uint64_t *a, uint64_t *b);
inline void heap_adjust_down_ss_simd(SpaceSavingSimdPtr ptr, uint32_t i);
inline void heap_adjust_up_ss_simd(SpaceSavingSimdPtr ptr, uint32_t i);
inline int find_in_heap_simd(SpaceSavingSimdPtr ptr, uint32_t key);

typedef uint32_t Counter;

struct ModuleSimdBatchSpaceSavingSimd {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    SpaceSavingSimdPtr spacesavingsimd;

    SpaceSavingSimdPtr spacesavingsimd_ptr1;
    SpaceSavingSimdPtr spacesavingsimd_ptr2;
};

typedef struct ModuleSimdBatchSpaceSavingSimd* ModuleSimdBatchSpaceSavingSimdPtr;

ModulePtr simdbatch_spacesavingsimd_init(ModuleConfigPtr);

void simdbatch_spacesavingsimd_delete(ModulePtr);
void simdbatch_spacesavingsimd_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void simdbatch_spacesavingsimd_reset(ModulePtr);
void simdbatch_spacesavingsimd_stats(ModulePtr, FILE *f);

#endif

