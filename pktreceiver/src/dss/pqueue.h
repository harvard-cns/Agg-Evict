#ifndef _PQUEUE_H_
#define _PQUEUE_H_

#include <inttypes.h>

typedef struct PriorityQueue* PriorityQueuePtr;
typedef uint32_t pqueue_index_t;

typedef int (*pqueue_comp_func)(void const*, void const *);

PriorityQueuePtr pqueue_create(unsigned size, uint16_t keysize,
        uint16_t elsize, unsigned socket, pqueue_comp_func func);
void pqueue_reset(PriorityQueuePtr);
void* pqueue_get_copy_key(PriorityQueuePtr, void const *key, pqueue_index_t *idx);
void pqueue_heapify_index(PriorityQueuePtr, pqueue_index_t idx);
void pqueue_swim(PriorityQueuePtr, pqueue_index_t idx);
void pqueue_sift(PriorityQueuePtr, pqueue_index_t idx);
void pqueue_delete(PriorityQueuePtr);
void *pqueue_peak(PriorityQueuePtr);
void pqueue_print(PriorityQueuePtr pq);
void *pqueue_iterate(PriorityQueuePtr pq, pqueue_index_t *);
void pqueue_assert_correctness(PriorityQueuePtr pq);
uint32_t pqueue_num_searches(PriorityQueuePtr pq);

#endif
