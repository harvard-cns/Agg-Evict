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
#include "../../dss/hashmap_linear.h"

struct CountHeap
{
	uint32_t rowsize;

    uint32_t capacity;
    uint32_t heap_element_num;

    int *row1;
    int *row2;
    int *row3;

    PairPtr heap;
    HashMapLinearPtr ht;

    uint32_t table[];
};
// count sketch: the number of counters in each row is rowsize;
// Hashtable: key --> uint32_t (flow ID); value --> uint32_t (pos in heap array);
// Heap: freq and flow ID;


inline void count_heap_reset(CountHeapPtr ptr)
{
	ptr->heap_element_num = 0;
    hashmap_linear_reset(ptr->ht);
	memset(ptr->table, 0, 3 * ptr->rowsize * sizeof(uint32_t) + 4 * ptr->capacity * sizeof(uint32_t));
}

CountHeapPtr count_heap_create(uint32_t rowsize, uint32_t capacity)
{

	/* Size up so that we are a power of two ... no wasted space */
    rowsize = rte_align32pow2(rowsize);

    CountHeapPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct CountHeap) + /* DS */
        3 * rowsize * sizeof(uint32_t) + 4 * capacity * sizeof(uint32_t), /* Size of elements */
        64, 0);

    ptr->rowsize = rowsize;
    ptr->capacity = capacity;
    ptr->heap_element_num = 0;

    ptr->row1 = (int *)(ptr->table);
    ptr->row2 = (int *)(ptr->table + rowsize);
    ptr->row3 = (int *)(ptr->table + rowsize * 2);

    ptr->heap = (PairPtr)(ptr->table + rowsize * 3);
    // each epoch has 0.14M flows, we use 0.2M entry in the hash table. 
    ptr->ht = hashmap_linear_create(200000, 2, 1, 0);
    return ptr;
}

inline void swap_heap_element(PairPtr a, PairPtr b)
{
    struct Pair t;
    t = *a;
    *a = *b;
    *b = t;
}

inline int query_hash(CountHeapPtr ptr, uint64_t key)
{
    uint32_t *find_res = hashmap_linear_get_nocopy_key(ptr->ht, &key);
    if(*(uint64_t*)(find_res - 2) == 0 || (*find_res) == (uint32_t)(1 << 31))
    {
        return -1;
    }
    return (*find_res);
}
inline void insert_hash(CountHeapPtr ptr, uint64_t key, uint32_t value)
{
    uint32_t* find_res = hashmap_linear_get_copy_key(ptr->ht, &key);
    (*find_res) = value;
}
inline void delete_hash(CountHeapPtr ptr, uint64_t key)
{
    uint32_t *find_res = hashmap_linear_get_nocopy_key(ptr->ht, &key);
    (*find_res) = (1 << 31);
}
inline void swap_hash(CountHeapPtr ptr, uint64_t key1, uint64_t key2)
{
    uint32_t* find_res1 = hashmap_linear_get_nocopy_key(ptr->ht, &key1);
    uint32_t* find_res2 = hashmap_linear_get_nocopy_key(ptr->ht, &key2);

    uint32_t t = *find_res1;
    *find_res1 = *find_res2;
    *find_res2 = t;
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
            swap_hash(ptr, heap[i].flowid, heap[larger_one].flowid);            
            swap_heap_element(&(heap[i]), &(heap[larger_one]));
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
        swap_hash(ptr, heap[i].flowid, heap[parent].flowid);
        swap_heap_element(&(heap[i]), &(heap[parent]));
            
        i = parent;
    }
}
inline int cmpfunc(const void * a, const void * b)
{
   return (*(int const*)a - *(int const*)b);
}

// inline int query_hash(CountHeapPtr ptr, uint64_t key)
// {
 
    // PairPtr heap = ptr->heap;
    // int now_heap_ele = ptr->heap_element_num;
    // for(int i = 0; i < now_heap_ele; i++)
    // {
    //     if(heap[i].flowid == key)
    //         return i;
    // }
    // return -1;
// }

inline void count_heap_insert(CountHeapPtr ptr, uint64_t key, uint32_t freq)
{
	int rowsize = ptr->rowsize;

	uint32_t h1 = dss_hash_with_seed((const void*)&key, 2, CMS_SH1) & (rowsize - 1);
    int *p1 = ptr->row1 + h1;
    rte_prefetch0(p1);

    uint32_t h2 = dss_hash_with_seed((const void*)&key, 2, CMS_SH2 + CMS_SH1) & (rowsize - 1);
    int *p2 = ptr->row2 + h2;
    rte_prefetch0(p2);

    uint32_t h3 = dss_hash_with_seed((const void*)&key, 2, CMS_SH3 + CMS_SH2) & (rowsize - 1);
    int *p3 = ptr->row3 + h3;
    rte_prefetch0(p3);

	// uint32_t h1_polar = dss_hash_with_seed((const void*)&key, 2, CMS_SH5) & 1;
    // uint32_t h2_polar = dss_hash_with_seed((const void*)&key, 2, CMS_SH5 + CMS_SH6) & 1;
    // uint32_t h3_polar = dss_hash_with_seed((const void*)&key, 2, CMS_SH5 + CMS_SH7) & 1;


    (*p1) ++;
    (*p2) ++;
    (*p3) ++;

    int tmin = 1 << 30;
    tmin = (*p1) < tmin ? (*p1) : tmin;
    tmin = (*p2) < tmin ? (*p2) : tmin;
    tmin = (*p3) < tmin ? (*p3) : tmin;



	PairPtr heap = ptr->heap;
	
    // return the pos of element in the heap; 
    int ret = query_hash(ptr, key);

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
        
        insert_hash(ptr, key, ptr->heap_element_num);

        (ptr->heap_element_num)++;

        heap_adjust_up(ptr, (ptr->heap_element_num) - 1);
    }
    else if(tmin > (int)(heap[0].freq))
    {
        delete_hash(ptr, heap[0].flowid);
        heap[0].flowid = key;
        heap[0].freq = tmin;
        insert_hash(ptr, key, 0);

        heap_adjust_down(ptr, 0);
    }
}

inline void count_heap_delete(CountHeapPtr ptr)
{
	rte_free(ptr);
}

inline void count_heap_report(CountHeapPtr ptr, FILE *univmon_res)
{
    PairPtr heap = ptr->heap;
    int now_heap_ele = ptr->heap_element_num;
    fprintf(univmon_res, "%d;", now_heap_ele);
    for(int i = 0; i < now_heap_ele; i++)
    {
        fprintf(univmon_res, "%llu,%u;", (long long unsigned int)heap[i].flowid, heap[i].freq);
    }

    int* p1 = ptr->row1;
    int* p2 = ptr->row2;
    int* p3 = ptr->row3;

    fprintf(univmon_res, "%d;", ptr->rowsize);
    for(uint32_t i = 0; i < ptr->rowsize; i++)
    {
        fprintf(univmon_res, "%d;", p1[i]);
    }
    for(uint32_t i = 0; i < ptr->rowsize; i++)
    {
        fprintf(univmon_res, "%d;", p2[i]);
    }
    for(uint32_t i = 0; i < ptr->rowsize; i++)
    {
        fprintf(univmon_res, "%d;", p3[i]);
    }


    // fwrite(heap, sizeof(Pair), now_heap_ele, univmon_res);
    // fprintf(univmon_res, "\n");
}

