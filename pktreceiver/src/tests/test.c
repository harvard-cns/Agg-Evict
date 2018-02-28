#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../dss/pqueue.h"

#include "test.h"

// Comparison function for PQE
static int test_comp_func(void const *a, void const *b) {
    uint32_t ai =*(uint32_t const *)a;
    uint32_t bi =*(uint32_t const *)b;

    return -((ai >= bi) - (ai <= bi));
}

// Priority queue element
typedef struct {
    uint32_t counter;
    uint32_t a; uint32_t b; uint32_t c;
} PQE;

static int test_pqueue(void) {
    uint32_t i;
    printf("Running PQueue test\n");
    PriorityQueuePtr pq = pqueue_create(32768, 2, 4, 1, test_comp_func);
    srand(0);

    uint64_t last_key = 0;
    uint32_t lc = 0xffff;

    for (i = 0; i < lc; ++i) {
        uint64_t key = last_key++;
        pqueue_index_t idx = 0;
        PQE *data = (PQE*)pqueue_get_copy_key(pq, &key, &idx);
        memset(data, 0, sizeof(PQE));
        data->counter = rand() & 0xffff;
        pqueue_heapify_index(pq, idx);
    }
    pqueue_assert_correctness(pq);

    int j = 0;

    for (j = 0; j < 100; ++j) {
        last_key = 0;
        for (i = 0; i < lc; ++i) {
            uint64_t key = rand() % 0xffff;
            pqueue_index_t idx = 0;
            PQE *data = (PQE*)pqueue_get_copy_key(pq, &key, &idx);
            data->counter += rand() & 0xf;
            pqueue_heapify_index(pq, idx);
        }
    }
    pqueue_assert_correctness(pq);
    pqueue_delete(pq);

    return 1;
}

int tests_run(void) {
    (void)(test_pqueue);
    test_pqueue();
    return 1;
}
