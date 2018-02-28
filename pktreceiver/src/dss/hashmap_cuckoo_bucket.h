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

#ifndef _HASHMAP_CUCKOO_BUCKET_H_
#define _HASHMAP_CUCKOO_BUCKET_H_

typedef struct HashMapCuckooBucket* HashMapCuckooBucketPtr; 

/* Create a new hashmap ... row and key sizes are in multiples of 4 bytes -- for
 * better performance due to alignment */
HashMapCuckooBucketPtr hashmap_cuckoo_bucket_create(uint32_t, uint16_t, uint16_t, int);

inline void *hashmap_cuckoo_bucket_get_copy_key(HashMapCuckooBucketPtr, void const *);
inline void *hashmap_cuckoo_bucket_get_nocopy_key(HashMapCuckooBucketPtr, void const *);
inline void *hashmap_cuckoo_bucket_get_with_hash(HashMapCuckooBucketPtr, uint32_t);
inline void  hashmap_cuckoo_bucket_delete(HashMapCuckooBucketPtr);

inline void *hashmap_cuckoo_bucket_begin(HashMapCuckooBucketPtr);
inline void *hashmap_cuckoo_bucket_end(HashMapCuckooBucketPtr);
inline void *hashmap_cuckoo_bucket_next(HashMapCuckooBucketPtr, void *);

uint32_t    hashmap_cuckoo_bucket_size(HashMapCuckooBucketPtr);
uint32_t    hashmap_cuckoo_bucket_count(HashMapCuckooBucketPtr);
inline void hashmap_cuckoo_bucket_reset(HashMapCuckooBucketPtr);

uint32_t hashmap_cuckoo_bucket_num_searches(HashMapCuckooBucketPtr);

#endif // _HASHMAP_CUCKOO_H_

