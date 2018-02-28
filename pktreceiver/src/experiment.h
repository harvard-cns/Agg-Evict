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

#ifndef _EXPERIMENT_H_
#define _EXPERIMENT_H_

typedef struct Experiments* ExprsPtr;
typedef struct ExpModuleConfig ModuleConfig;
typedef ModuleConfig* ModuleConfigPtr;

#include "net.h"

ExprsPtr expr_parse(const char *fname);

void expr_initialize(ExprsPtr, PortPtr );
void expr_cleanup(ExprsPtr);
void expr_signal(ExprsPtr);
void expr_stats_save(ExprsPtr, FILE *);

uint32_t    mc_uint32_get  (ModuleConfigPtr, char const *);
unsigned    mc_unsigned_get(ModuleConfigPtr, char const *);
char const *mc_string_get  (ModuleConfigPtr, char const *);


#endif
