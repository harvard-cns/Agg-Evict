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

#ifndef _MODULE_H_
#define _MODULE_H_

#include "common.h"
#include "experiment.h"
#include "net.h"

typedef struct Module* ModulePtr;
typedef void (*ModuleFunc)(ModulePtr, PortPtr, \
        struct rte_mbuf ** __restrict__, uint32_t);

struct Module {
    const char name[MAX_MODULE_NAME];

    ModuleFunc execute;
};

typedef ModulePtr (*ModuleInit) (ModuleConfigPtr);
typedef void (*ModuleDelete)    (ModulePtr);
typedef void (*ModuleExecute)   (ModulePtr, PortPtr, struct rte_mbuf **, uint32_t);
typedef void (*ModuleReset)     (ModulePtr);
typedef void (*ModuleStats)     (ModulePtr, FILE*);

struct ModuleList {
    char name[255];

    ModuleInit    init;
    ModuleDelete  del;
    ModuleExecute execute;
    ModuleReset   reset;
    ModuleStats   stats;

    struct ModuleList *next;
};

extern struct ModuleList _all_modules;

void _module_register(
        char const* name, ModulePtr (*init) (ModuleConfigPtr),
        void (*del)(ModulePtr),
        void (*execute)(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t),
        void (*reset)(ModulePtr),
        void (*stats)(ModulePtr, FILE*));

ModuleInit    module_init_get(char const* name);
ModuleDelete  module_delete_get(char const* name);
ModuleExecute module_execute_get(char const* name);
ModuleReset   module_reset_get(char const* name);
ModuleStats   module_stats_get(char const* name);

#define REGISTER_MODULE(name, functor) _module_register(name, functor##_init, \
        functor##_delete, functor##_execute, functor##_reset, functor##_stats);

#endif // _MODULE_H_
