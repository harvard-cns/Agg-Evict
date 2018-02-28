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

#ifndef _SUPER_SPREADER_H_
#define _SUPER_SPREADER_H_

#include "../common.h"
#include "../module.h"
#include "../net.h"

struct SSField {
    uint32_t src;
    uint64_t bits;
    uint32_t count;
};
struct ModuleSuperSpreader {
    struct Module _m;

    uint32_t  size;
    uint32_t  hashes[MAX_PKT_BURST];
    struct SSField table[];
};

typedef struct ModuleSuperSpreader* ModuleSuperSpreaderPtr;

ModuleSuperSpreaderPtr super_spreader_init(uint32_t);
void super_spreader_delete(ModulePtr);
void super_spreader_execute(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);

#endif
