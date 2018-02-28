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

#ifndef _MODULE_RING_H_
#define _MODULE_RING_H_

#include "rte_ring.h"

#include "../common.h"
#include "../module.h"
#include "../net.h"

struct ModuleRingHeader {
    uint32_t srcip;
    uint32_t dstip;

    uint16_t srcport;
    uint16_t dstport; 

    uint32_t hash;
};

typedef uint32_t Counter;
struct ModuleRing {
    struct Module _m;

    uint32_t         size;
    struct rte_ring *ring;

    uint8_t          el_size;
    uint8_t          socket_id;
    uint32_t         table_idx;

    struct ModuleRingHeader table[];
};

typedef struct ModuleRing* ModuleRingPtr;

ModuleRingPtr ring_init(uint32_t, uint32_t, uint8_t, uint8_t);
void ring_delete(ModulePtr);
void ring_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
struct rte_ring *ring_get_ring(ModuleRingPtr);

#endif
