#include "../../common.h"
#include "../../vendor/murmur3.h"
#include "../../dss/hash.h"

#include <stdio.h>
#include <stdlib.h>

#include "rte_atomic.h"
#include "rte_malloc.h"
#include "rte_memcpy.h"

#include "countheap.h"
#include "common.h"

struct CountHeap
{
	uint32_t rowsize;

    uint32_t capacity;
    uint32_t heap_element_num;

    int *row1;
    int *row2;
    int *row3;
    int *row4;

    PairPtr heap;

    uint32_t table[];
};
// count sketch: the number of counters in each row is rowsize;
// Hashtable: key --> uint32_t (flow ID); value --> uint32_t (pos in heap array);
// Heap: freq and flow ID;


inline void count_heap_reset(CountHeapPtr ptr)
{
	ptr->heap_element_num = 0;
	memset(ptr->table, 0, 4 * ptr->rowsize * sizeof(uint32_t) + 2 * ptr->capacity * sizeof(uint32_t));
}

CountHeapPtr count_heap_create(uint32_t rowsize, uint32_t capacity)
{

	/* Size up so that we are a power of two ... no wasted space */
    rowsize = rte_align32pow2(rowsize);

    CountHeapPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct CountHeap) + /* DS */
        4 * rowsize * sizeof(uint32_t) + 2 * capacity * sizeof(uint32_t), /* Size of elements */
        64, 0);

    ptr->rowsize = rowsize;
    ptr->capacity = capacity;
    ptr->heap_element_num = 0;

    ptr->row1 = (int *)(ptr->table);
    ptr->row2 = (int *)(ptr->table + rowsize);
    ptr->row3 = (int *)(ptr->table + rowsize * 2);
    ptr->row4 = (int *)(ptr->table + rowsize * 3);

    ptr->heap = (PairPtr)(ptr->table + rowsize * 4);

    return ptr;
}

inline void swap_32(uint32_t *a, uint32_t *b)
{
    uint32_t t;
    t = *a;
    *a = *b;
    *b = t;
}

inline void swap_64(uint64_t *a, uint64_t *b)
{
    uint64_t t;
    t = *a;
    *a = *b;
    *b = t;
}

inline void heap_adjust_down(CountHeapPtr ptr, uint32_t i) 
{
	PairPtr heap = ptr->heap;

	uint32_t heap_element_num = ptr->heap_element_num;
	
	while (i < heap_element_num / 2) 
	{
        uint32_t l_child = 2 * i + 1;
        uint32_t r_child = 2 * i + 2;
        uint32_t larger_one = i;
        if (l_child < heap_element_num && heap[l_child].freq < heap[larger_one].freq) 
        {
            larger_one = l_child;
        }
        if (r_child < heap_element_num && heap[r_child].freq < heap[larger_one].freq) 
        {
            larger_one = r_child;
        }
        if (larger_one != i) 
        {
            swap_64((uint64_t*)&(heap[i]), (uint64_t*)&(heap[larger_one]));
            heap_adjust_down(ptr, larger_one);
        } 
        else 
        {
            break;
        }
    }
}
inline void heap_adjust_up(CountHeapPtr ptr, uint32_t i) 
{
	PairPtr heap = ptr->heap;
	
    while (i > 1) 
    {
        uint32_t parent = (i - 1) / 2;
        if (heap[parent].freq <= heap[i].freq) 
        {
            break;
        }
        swap_64((uint64_t*)&(heap[i]), (uint64_t*)&(heap[parent]));
            
        i = parent;
    }
}
inline int cmpfunc(const void * a, const void * b)
{
   return (*(int const*)a - *(int const*)b);
}
inline int find_in_heap_ch(CountHeapPtr ptr, uint32_t key)
{
    PairPtr heap = ptr->heap;
    int now_heap_ele = ptr->heap_element_num;
    for(int i = 0; i < now_heap_ele; i++)
    {
        if(heap[i].flowid == key)
            return i;
    }
    return -1;
}

inline void count_heap_insert(CountHeapPtr ptr, uint32_t key, uint32_t freq)
{
	int rowsize = ptr->rowsize;

	uint32_t h1 = dss_hash_with_seed((const void*)&key, 1, CMS_SH1) & (rowsize - 1);
    int *p1 = ptr->row1 + h1;
    rte_prefetch0(p1);

    uint32_t h2 = dss_hash_with_seed((const void*)&key, 1, CMS_SH2 + CMS_SH1) & (rowsize - 1);
    int *p2 = ptr->row2 + h2;
    rte_prefetch0(p2);

    uint32_t h3 = dss_hash_with_seed((const void*)&key, 1, CMS_SH3 + CMS_SH2) & (rowsize - 1);
    int *p3 = ptr->row3 + h3;
    rte_prefetch0(p3);

    uint32_t h4 = dss_hash_with_seed((const void*)&key, 1, CMS_SH4 + CMS_SH3) & (rowsize - 1);
    int *p4 = ptr->row4 + h4;
    rte_prefetch0(p4);


	uint32_t h1_polar = dss_hash_with_seed((const void*)&key, 1, CMS_SH5) & 1;
    uint32_t h2_polar = dss_hash_with_seed((const void*)&key, 1, CMS_SH5 + CMS_SH6) & 1;
    uint32_t h3_polar = dss_hash_with_seed((const void*)&key, 1, CMS_SH5 + CMS_SH7) & 1;
    uint32_t h4_polar = dss_hash_with_seed((const void*)&key, 1, CMS_SH5 + CMS_SH8) & 1;


    (*p1) += h1_polar ? freq : -freq;
    (*p2) += h2_polar ? freq : -freq;
    (*p3) += h3_polar ? freq : -freq;
    (*p4) += h4_polar ? freq : -freq;

    int ans[4];
    ans[0] = h1_polar ? (*p1) : -(*p1);
    ans[1] = h2_polar ? (*p2) : -(*p2);
    ans[2] = h3_polar ? (*p3) : -(*p3);
    ans[3] = h4_polar ? (*p4) : -(*p4);

    qsort(ans, 4, sizeof(int), cmpfunc);
	
	int tmin = (ans[1] + ans[2]) / 2;
    


	PairPtr heap = ptr->heap;
	
    // return the pos of element in the heap; 
    int ret = find_in_heap_ch(ptr, key);

    // check the key part, because the pos part maybe 0 (the first element in the heap);
    if(ret != -1)
    {
    	heap[ret].freq += freq;
    	heap_adjust_down(ptr, ret);
    }
    else if(ptr->heap_element_num < ptr->capacity)
    {
    	heap[ptr->heap_element_num].flowid = key;
        heap[ptr->heap_element_num].freq = tmin;
        
        (ptr->heap_element_num)++;

        heap_adjust_up(ptr, (ptr->heap_element_num) - 1);
    }
    else if(tmin > (int)(heap[0].freq))
    {
        heap[0].flowid = key;
        heap[0].freq = tmin;
        heap_adjust_down(ptr, 0);
    }
}

inline void count_heap_delete(CountHeapPtr ptr)
{
	rte_free(ptr);
}

