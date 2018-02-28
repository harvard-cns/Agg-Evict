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

#ifndef _MEMORY_H_
#define _MEMORY_H_

typedef struct HugePage* HugePagePtr;

/* Allocate a single huge page for saving objects */
HugePagePtr mem_create(int);

/* Allocate a space of size_t on a huge page. 
 *
 * - If HugePagePtr is use a static huge page allocated on the same socket id as
 * the calling thread.
 *
 * - If the huge page is full, return null.
 *
 * Finally, make sure that the returned pointer is cache-aligned or at least in
 * the same cache line as the last object.
 */
int mem_alloc(size_t, HugePagePtr, void **);

/* Delete a huge page container */
int mem_delete(HugePagePtr);

#endif // _MEMORY_H_
