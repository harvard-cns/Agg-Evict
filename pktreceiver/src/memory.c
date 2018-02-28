#include <stdlib.h>

#include <rte_malloc.h>
#include <rte_lcore.h>
#include <rte_log.h>

#include "common.h"
#include "memory.h"

static void mem_delete_static_pages(void);

struct HugePage {
    char *start;
    char *ptr;
    int socket_id;
};

/* Hold a huge page per socket */
static HugePagePtr gb[MAX_NUM_SOCKETS] = {0};

HugePagePtr
mem_create(int socket_id) {
    if (socket_id == -1)
        socket_id = rte_socket_id();

    HugePagePtr page = (HugePagePtr)rte_calloc_socket(
            NULL, 1, HUGE_PAGE_SIZE, HUGE_PAGE_SIZE+1, socket_id);

    if (!page)
        return 0;

    page->start = ((char*)page);
    page->ptr = page->start + sizeof(struct HugePage);
    page->socket_id = socket_id;

    return page;
}

inline int
mem_delete(HugePagePtr mem) {
    rte_free(mem);
    return 0;
}

int
mem_alloc(size_t size, HugePagePtr mem, void **ptr) {
    /* Allocate a static page on the core if there is not enough memory */
    if (mem == NULL) {
        int socket_id = rte_socket_id();
        if (!gb[socket_id]) {
            gb[socket_id] = mem_create(socket_id);
            atexit(mem_delete_static_pages);
        }
        mem = gb[socket_id];
    }

    /* Align to cache size */
    char *start = mem->ptr;
    if (size < 64) {
        /* See if the structure doesn't fit in the same cache line */
        if (((uintptr_t)(start) & 0x3f) + size > 64) {
            start = (char *)((((uintptr_t)start + 64) >> 6) << 6);
        }
    }

    /* Check to see if we have enough space left in the page */
    if (start + size > mem->start + HUGE_PAGE_SIZE)
        return ENOMEM;

    mem->ptr = start + size;
    *ptr = start;

    return 0;
}

static void
mem_delete_static_pages(void) {
    int i = 0;
    for (i = 0; i < MAX_NUM_SOCKETS; ++i) {
        if (gb[i] != 0) {
            RTE_LOG(DEBUG, USER1,
                    "Releasing socket huge pages %d\n", i);
            mem_delete(gb[i]);
        }
    }
}
