/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Omid Alipourfard <g@omid.io>
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

#ifndef _RANDMOD_H_
#define _RANDMOD_H_

#include "../common.h"
#include "../experiment.h"
#include "../module.h"
#include "../net.h"
#include "../reporter.h"

/*
 * Random access module
 *
 * A useless module that randomly accesses data per packet
 *
 */
struct ModuleRandMod {
    struct Module _m;

    uint32_t  size;        // Size of the data-structure in uint32_t
    unsigned  access_size; // Amount of data accessed per packet in uint32_t
    unsigned  socket;
    uint32_t  idx;

    uint8_t values[];
};

typedef struct ModuleRandMod* ModuleRandModPtr;

ModulePtr randmod_init(ModuleConfigPtr);

void randmod_delete(ModulePtr);
void randmod_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
void randmod_reset(ModulePtr);
void randmod_stats(ModulePtr, FILE *f);

#endif
