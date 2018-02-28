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

#ifndef _HEAVYCHANGE_REVSKETCH_H
#define _HEAVYCHANGE_REVSKETCH_H

#include "../../common.h"
#include "../../experiment.h"
#include "../../module.h"
#include "../../net.h"
#include "common.h"
#include "../../dss/hashmap_linear.h"


struct RevSketch {
    uint32_t size;
    uint32_t rowsize;
    uint16_t keysize;
    uint16_t elsize;

    rte_atomic32_t stats_search;

    HashMapLinearPtr ht;

    uint32_t *row1;
    // uint32_t *row2;
    // uint32_t *row3;

    uint32_t *row1_last;
    // uint32_t *row2_last;
    // uint32_t *row3_last;


    // uint32_t epoch_id;

    uint32_t table[];
};

typedef struct RevSketch* RevSketchPtr; 

/* Create a new revsketch ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
RevSketchPtr revsketch_create(uint32_t, uint16_t, uint16_t, int);

inline int  revsketch_inc(RevSketchPtr, void const *);
inline void revsketch_delete(RevSketchPtr);
inline void revsketch_reset(RevSketchPtr);

uint32_t revsketch_num_searches(RevSketchPtr);





typedef uint32_t Counter;

struct ModuleHeavyChangeRevSketch {
    struct Module _m;

    uint32_t  size;
    unsigned  keysize;
    unsigned  elsize;
    unsigned  socket;

    unsigned stats_search;

    RevSketchPtr revsketch;

    RevSketchPtr revsketch_ptr1;
    RevSketchPtr revsketch_ptr2;
};

typedef struct ModuleHeavyChangeRevSketch* ModuleHeavyChangeRevSketchPtr;

ModulePtr heavychange_revsketch_init(ModuleConfigPtr);

void heavychange_revsketch_delete(ModulePtr);
void heavychange_revsketch_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void heavychange_revsketch_reset(ModulePtr);
void heavychange_revsketch_stats(ModulePtr, FILE *f);

#endif

