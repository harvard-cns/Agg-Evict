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

#ifndef _SIMDBATCH_SPACESAVING_H
#define _SIMDBATCH_SPACESAVING_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "common.h"

struct SpaceSaving {
    uint32_t size;
    uint16_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;


    uint32_t now_element;
    NodePtr nodes;
    uint32_t table[];
};

typedef struct SpaceSaving* SpaceSavingPtr; 

/* Create a new spacesaving ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
SpaceSavingPtr spacesaving_create(uint32_t, uint16_t, uint16_t, int);

inline int  spacesaving_inc(SpaceSavingPtr, void const *);
inline void spacesaving_delete(SpaceSavingPtr);
inline void spacesaving_reset(SpaceSavingPtr);

inline void heap_adjust_down_ss(SpaceSavingPtr ptr, uint32_t i);
inline void heap_adjust_up_ss(SpaceSavingPtr ptr, uint32_t i);
inline void swap_32_ss(uint32_t *a, uint32_t *b);
inline void swap_64_ss(uint64_t *a, uint64_t *b);
inline int find_in_heap(SpaceSavingPtr ptr, uint32_t key);

uint32_t spacesaving_num_searches(SpaceSavingPtr);





typedef uint32_t Counter;

struct ModuleSimdBatchSpaceSaving {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    SpaceSavingPtr spacesaving;

    SpaceSavingPtr spacesaving_ptr1;
    SpaceSavingPtr spacesaving_ptr2;
};

typedef struct ModuleSimdBatchSpaceSaving* ModuleSimdBatchSpaceSavingPtr;

ModulePtr simdbatch_spacesaving_init(ModuleConfigPtr);

void simdbatch_spacesaving_delete(ModulePtr);
void simdbatch_spacesaving_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void simdbatch_spacesaving_reset(ModulePtr);
void simdbatch_spacesaving_stats(ModulePtr, FILE *f);

#endif

