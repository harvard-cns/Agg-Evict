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

#ifndef _HASHMAP_CUCKOO_H_
#define _HASHMAP_CUCKOO_H_

/* Implementation of a hashmap with no collision resolution. If collisions
 * happen we simply overwrite */
typedef struct HashMapCuckoo* HashMapCuckooPtr; 

/* Create a new hashmap ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
HashMapCuckooPtr hashmap_cuckoo_create(uint32_t, uint16_t, uint16_t, int);

inline void *hashmap_cuckoo_get_copy_key(HashMapCuckooPtr, void const *);
inline void *hashmap_cuckoo_get_nocopy_key(HashMapCuckooPtr, void const *);
inline void *hashmap_cuckoo_get_with_hash(HashMapCuckooPtr, uint32_t);
inline void hashmap_cuckoo_delete(HashMapCuckooPtr);

inline void *hashmap_cuckoo_begin(HashMapCuckooPtr);
inline void *hashmap_cuckoo_end(HashMapCuckooPtr);
inline void *hashmap_cuckoo_next(HashMapCuckooPtr, void *);

uint32_t hashmap_cuckoo_size(HashMapCuckooPtr);
uint32_t hashmap_cuckoo_count(HashMapCuckooPtr);
inline void hashmap_cuckoo_reset(HashMapCuckooPtr);

uint32_t hashmap_cuckoo_num_searches(HashMapCuckooPtr);

#endif // _HASHMAP_CUCKOO_H_

